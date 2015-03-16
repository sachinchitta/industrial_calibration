/*
 * Software License Agreement (Apache License)
 *
 * Copyright (c) 2014, Southwest Research Institute
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ros/ros.h>
#include <ros/package.h>
#include <ros/console.h>
#include <moveit/move_group_interface/move_group.h>
#include <actionlib/server/simple_action_server.h>
#include <industrial_extrinsic_cal/robot_joint_values_triggerAction.h> // one of the ros action messages
#include <industrial_extrinsic_cal/robot_pose_triggerAction.h> // the other ros action message

class ServersNode {
protected:
  ros::NodeHandle nh_;

public:
  
  typedef actionlib::SimpleActionServer<industrial_extrinsic_cal::robot_joint_values_triggerAction> JointValuesServer;
  typedef actionlib::SimpleActionServer<industrial_extrinsic_cal::robot_pose_triggerAction> PoseServer;

  ServersNode(std::string name) :
    joint_value_server_(nh_,name + "_joint_values",boost::bind(&ServersNode::jointValueCallBack, this, _1), false),
    pose_server_(nh_,name +"_pose",boost::bind(&ServersNode::poseCallBack, this, _1), false),
    action_name_(name)
  {
    std::string group_name;
    nh_.param<std::string>(name+"/group_name", group_name, "Manipulator");
    
    joint_value_server_.start();
    pose_server_.start();
    ROS_INFO("Loading move group interface for group: %s", group_name.c_str());
    move_group_ = new moveit::planning_interface::MoveGroup(group_name);
    move_group_->setPlanningTime(10.0); // give it 10 seconds to plan
    move_group_->setNumPlanningAttempts(30.0); // Allow parallel planner to hybridize this many plans
    move_group_->setPlannerId("RRTConnectkConfigDefault"); // use this planner

  };

  ~ServersNode() 
  {
    delete(move_group_);
  };

  void jointValueCallBack(const industrial_extrinsic_cal::robot_joint_values_triggerGoalConstPtr& goal)
  {
    // TODO send both values and names and make sure they match. This is critical or else robots will crash into stuff
    std::vector<double> group_variable_values;
    moveit::core::RobotStatePtr current_state = move_group_->getCurrentState();
    double *joint_value = current_state->getVariablePositions();
    std::vector<std::string> var_names = current_state->getVariableNames();
    ROS_DEBUG("%d variables in start state %s %s %s %s %s %s %s", (int)var_names.size(),
	      var_names[0].c_str(),
	      var_names[1].c_str(),
	      var_names[2].c_str(),
	      var_names[3].c_str(),
	      var_names[4].c_str(),
	      var_names[5].c_str(),
	      var_names[6].c_str());
    move_group_->setStartState(*current_state);
    if(!move_group_->setJointValueTarget(goal->joint_values))
    {
      ROS_ERROR("Could not set joint value targets");
      return;
    }
    for(std::size_t i=0; i < goal->joint_values.size(); ++i)
    {
      ROS_DEBUG("Moving joint %d to %f", (int) i, goal->joint_values[i]);
    }
    if(move_group_->move()){
      ROS_INFO("Move succeeded");
      sleep(3);
      joint_value_server_.setSucceeded();
    }
    else{
      ROS_ERROR("move in jointValueCallBack failed");
    }
  };
  
  void poseCallBack(const industrial_extrinsic_cal::robot_pose_triggerGoalConstPtr& goal)
  {
    moveit::core::RobotStatePtr current_state = move_group_->getCurrentState();
    move_group_->setStartState(*current_state);
    geometry_msgs::PoseStamped current_pose = move_group_->getCurrentPose();
    ROS_ERROR("starting pose = %lf  %lf  %lf  %lf  %lf  %lf  %lf",
	      current_pose.pose.position.x,
	      current_pose.pose.position.y,
	      current_pose.pose.position.z,
	      current_pose.pose.orientation.x,
	      current_pose.pose.orientation.y,
	      current_pose.pose.orientation.z,
	      current_pose.pose.orientation.w);
    geometry_msgs::Pose target_pose;
    target_pose.position.x = goal->pose.position.x;
    target_pose.position.y = goal->pose.position.y;
    target_pose.position.z = goal->pose.position.z;
    target_pose.orientation.w = goal->pose.orientation.x;
    target_pose.orientation.w = goal->pose.orientation.y;
    target_pose.orientation.w = goal->pose.orientation.z;
    target_pose.orientation.w = goal->pose.orientation.w;
    move_group_->setPoseTarget(target_pose);
    if(move_group_->move()){
      joint_value_server_.setSucceeded();
    }
    else {
      ROS_ERROR("move in poseCallBack failed");
    }
  };

private:
  JointValuesServer joint_value_server_;
  PoseServer pose_server_;
  std::string action_name_;
  moveit::planning_interface::MoveGroup *move_group_;
};

int main(int argc, char** argv)
{
  ros::init(argc, argv, "rosRobotActionTriggerServer");
  ServersNode Servers(ros::this_node::getName());
  ros::spin();
  return 0;
}
