//
// Created by nermin on 28.08.23.
//

#ifndef SIM_BRINGUP_ROBOT_H
#define SIM_BRINGUP_ROBOT_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <Eigen/Eigen>
#include <rclcpp/rclcpp.hpp>
#include <control_msgs/msg/joint_trajectory_controller_state.hpp>
#include <yaml-cpp/yaml.h>

#include <RealVectorSpaceState.h>
#include <xArm6.h>

namespace sim_bringup
{
    class Robot
    {
    public:
        Robot(const std::string &config_file_path);
        ~Robot() {}
        
        inline std::shared_ptr<robots::AbstractRobot> getRobot() const { return robot; }
        inline std::shared_ptr<base::State> getJointsPositionPtr() const 
            { return std::make_shared<base::RealVectorSpaceState>(joints_position); }
        inline std::shared_ptr<base::State> getJointsVelocityPtr() const 
            { return std::make_shared<base::RealVectorSpaceState>(joints_velocity); }
        inline std::shared_ptr<base::State> getJointsAccelerationPtr() const 
            { return std::make_shared<base::RealVectorSpaceState>(joints_acceleration); }
        inline std::shared_ptr<base::State> getHomeJointsStatePtr() const 
            { return std::make_shared<base::RealVectorSpaceState>(home_joints_position); }
        inline const Eigen::VectorXf &getJointsPosition() const { return joints_position; }
        inline const Eigen::VectorXf &getJointsVelocity() const { return joints_velocity; }
        inline const Eigen::VectorXf &getJointsAcceleration() const { return joints_acceleration; }
        inline const Eigen::VectorXf &getHomeJointsPosition() const { return home_joints_position; }

        inline float getMaxLinVel() const { return max_lin_vel; }
        inline float getMaxLinAcc() const { return max_lin_acc; }
        inline float getMaxLinJerk() const { return max_lin_jerk; }

        inline float getMaxVel(int num) const { return robot->getMaxVel(num); }
        inline float getMaxAcc(int num) const { return robot->getMaxAcc(num); }
        inline float getMaxJerk(int num) const { return robot->getMaxJerk(num); }
        inline int getNumDOFs() const { return num_DOFs; }

        void jointsStateCallback(const control_msgs::msg::JointTrajectoryControllerState::SharedPtr msg);
        bool isReady();
        bool isReached(std::shared_ptr<base::State> q, float tol = 0.01);
        
        rclcpp::Subscription<control_msgs::msg::JointTrajectoryControllerState>::SharedPtr joints_state_subscription;

    private:
        std::shared_ptr<robots::AbstractRobot> robot;
        Eigen::VectorXf joints_position;
        Eigen::VectorXf joints_velocity;
        Eigen::VectorXf joints_acceleration;
        Eigen::VectorXf home_joints_position;
        float max_lin_vel;      // in [m/s]
        float max_lin_acc;      // in [m/s²]
        float max_lin_jerk;     // in [m/s³]
        bool ready;
        int num_DOFs;
    };
}

#endif // SIM_BRINGUP_ROBOT_H