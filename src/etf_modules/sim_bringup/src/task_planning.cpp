#include "task_planning.h"

TaskPlanningNode::TaskPlanningNode(const std::string scenario_file_path, const std::string node_name, const int period, 
    const std::string time_unit) : PlanningNode(scenario_file_path, node_name, period, time_unit)
{
    task = 0;
}

void TaskPlanningNode::taskPlanningCallback()
{
    switch (task)
    {
    case 1:
        chooseObject();
        if (obj_idx != -1)
        {
            RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Object %d is recognized at the position (%f, %f, %f).", 
                obj_idx, objects_pos[obj_idx].x(), objects_pos[obj_idx].y(), objects_pos[obj_idx].z());
            computeObjectApproachAndPickAngles();

            scenario->setStart(std::make_shared<base::RealVectorSpaceState>(joint_states));
            scenario->setGoal(q_object_approach);
            task = 100;
            task_next = 2;
        }
        else
            task = 0;
        break;

    case 2:
        RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Going towards the object..."); 
        planner_path.clear();
        planner_path.emplace_back(q_object_approach);
        planner_path.emplace_back(q_object_pick);
        parametrizePath();
        publishTrajectory(path, path_times);
        task++;
        break;

    case 3:
        RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Picking the object...");
        task++;
        break;

    case 4:
        RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Raising the object...");
        planner_path.clear();
        planner_path.emplace_back(q_object_pick);
        planner_path.emplace_back(q_object_approach);
        parametrizePath();
        publishTrajectory(path, path_times);
        task++;
        break;

    case 5:
        RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Moving the object to destination...");
        scenario->setStart(std::make_shared<base::RealVectorSpaceState>(joint_states));
        scenario->setGoal(q_goal);
        task = 100;
        task_next = 6;
        break;
    
    case 6:
        RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Releasing the object...");
        task = 0;
        break;

    case 100:   // planning
        switch (state)
        {
        case 0:
            if (planner_ready)
            {
                RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Updating the environment..."); 
                updateEnvironment();

                RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Planning the path..."); 
                if (planPath())
                {
                    parametrizePath();
                    state++;
                }
            }
            else
                RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Waiting for the planner..."); 
            break;
        
        case 1:
            publishTrajectory(path, path_times);
            state = -1;   // Just to go to default
            break;

        default:
            RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Executing trajectory...");
            if ((joint_states - scenario->getGoal()->getCoord()).norm() < 0.1)
            {
                state = 0;
                task = task_next;
            }
            break;
        }
        break;

    default:
        RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Waiting for the object...");
        objects_pos.clear();
        objects_dim.clear();
        num_captures.clear();
        if (joint_states_ready)
            task = 1;   // Do measurements
        break;
    }
    RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "----------------------------------------------------------------\n");
}

void TaskPlanningNode::boundingBoxesCallbackWithFiltering(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
{
    if (task == 1 || task == 100)
        PlanningNode::boundingBoxesCallbackWithFiltering(msg);
}

void TaskPlanningNode::chooseObject()
{
    float z_max = -INFINITY;
    obj_idx = -1;
    for (int i = 0; i < objects_pos.size(); i++)
    {
        RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Object %d. dim = (%f, %f, %f), pos = (%f, %f, %f). Num. captures %d.",
            i, objects_dim[i].x(), objects_dim[i].y(), objects_dim[i].z(), 
               objects_pos[i].x(), objects_pos[i].y(), objects_pos[i].z(), num_captures[i]);
        if (num_captures[i] >= min_num_captures && objects_pos[i].z() > z_max)
        {
            z_max = objects_pos[i].z();
            obj_idx = i;
        }
    }
}

void TaskPlanningNode::computeObjectApproachAndPickAngles()
{
    // For approaching from above
    KDL::Vector n(objects_pos[obj_idx].x(), objects_pos[obj_idx].y(), 0); n.Normalize();
    KDL::Vector s(objects_pos[obj_idx].y(), -objects_pos[obj_idx].x(), 0); s.Normalize();
    KDL::Vector a(0, 0, -1);
    
    // For approaching by side
    // KDL::Vector n(0, 0, -1);
    // KDL::Vector s(-objects_pos[obj_idx].y(), objects_pos[obj_idx].x(), 0); s.Normalize();
    // KDL::Vector a(objects_pos[obj_idx].x(), objects_pos[obj_idx].y(), 0); a.Normalize();

    KDL::Rotation R(n, s, a);
    // RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Rotation matrix: ");
    // RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Vector n: (%f, %f, %f)", R(0,0), R(1,0), R(2,0));
    // RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Vector s: (%f, %f, %f)", R(0,1), R(1,1), R(2,1));
    // RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "Vector a: (%f, %f, %f)", R(0,2), R(1,2), R(2,2));
    
    float fi = std::atan2(objects_pos[obj_idx].y(), objects_pos[obj_idx].x());
    float r = objects_pos[obj_idx].head(2).norm();
    float r_crit = 0.3;
    if (r < r_crit)
        r += 1.5 * objects_dim[obj_idx].head(2).norm();
    else
        r -= 1.5 * objects_dim[obj_idx].head(2).norm();

    KDL::Vector p_approach = KDL::Vector(r * float(cos(fi)), 
                                         r * float(sin(fi)), 
                                         objects_pos[obj_idx].z() + offset_z + delta_z);
    KDL::Vector p_pick = KDL::Vector(objects_pos[obj_idx].x(), 
                                     objects_pos[obj_idx].y(), 
                                     objects_pos[obj_idx].z() + offset_z);    
    int num = 0;
    while (num++ <= 1000)
    {
        q_object_approach = robot->computeInverseKinematics(R, p_approach);
        
        // It is convenient for the purpose when picking objects from above
        if (r > r_crit && std::abs(q_object_approach->getCoord(3)) < 0.1 ||
            r <= r_crit && std::abs(q_object_approach->getCoord(3) - (-M_PI)) < 0.1)
            break;
    }
    q_object_pick = robot->computeInverseKinematics(R, p_pick, q_object_approach); // In order to ensure that 'q_object_pick' is relatively close to 'q_object_approach' 

    KDL::Vector p_goal = KDL::Vector(goal_pos.x(),
                                     goal_pos.y(),
                                     goal_pos.z() + objects_dim[obj_idx].z());
    KDL::Vector n_goal(goal_pos.x(), goal_pos.y(), 0); n.Normalize();
    KDL::Vector s_goal(goal_pos.y(), -goal_pos.x(), 0); s.Normalize();
    KDL::Vector a_goal(0, 0, -1);
    KDL::Rotation R_goal(n_goal, s_goal, a_goal);
    Eigen::VectorXf goal_angles = q_object_pick->getCoord();
    goal_angles(0) = std::atan2(goal_pos.y(), goal_pos.x());
    q_goal = robot->computeInverseKinematics(R_goal, p_goal, std::make_shared<base::RealVectorSpaceState>(goal_angles));
}

bool TaskPlanningNode::whetherToRemoveBoundingBox(Eigen::Vector3f &object_pos, Eigen::Vector3f &object_dim)
{
    // Filter the destination box from the scene
    if (object_pos.x() < -0.4 && std::abs(object_pos.y()) < 0.2 && object_pos.z() < 0.25)
        return true;
    
    return false;
}