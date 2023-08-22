#include "planning.h"

class TaskPlanningNode : public PlanningNode
{
public:
    TaskPlanningNode(const std::string scenario_file_path, const std::string node_name, const int period, 
                     const std::string time_unit = "milliseconds");

protected:
    void planningCallback() override { taskPlanningCallback(); }
    void taskPlanningCallback();
    void boundingBoxesCallbackWithFiltering(const sensor_msgs::msg::PointCloud2::SharedPtr msg) override;
    void chooseObject();
    void computeObjectApproachAndPickAngles();
    bool whetherToRemoveBoundingBox(Eigen::Vector3f &object_pos, Eigen::Vector3f &object_dim) override;

private:
    int task, task_next;
    std::shared_ptr<base::State> q_object_approach;
    std::shared_ptr<base::State> q_object_pick;
    std::shared_ptr<base::State> q_goal;
    int obj_idx;
    float delta_z = 0.1;
    float offset_z = 0;
    Eigen::Vector3f goal_pos = Eigen::Vector3f(-0.5, 0, 0.2);
};