#include <gtest/gtest.h>
#include <vector>
#include <string>

#include <ros/ros.h>
#include <eigen_conversions/eigen_msg.h>
#include <moveit_opw_kinematics_plugin/moveit_opw_kinematics_plugin.h>

const double TOLERANCE = 1e-6; // absolute tolerance for EXPECT_NEAR checks

template <typename T>
using Transform = Eigen::Transform<T, 3, Eigen::Affine>;

/** @brief Compare every element of two eigen affine3 poses.
 */
template <typename T>
void comparePoses(const Transform<T> & Ta, const Transform<T> & Tb)
{
  using Matrix = Eigen::Matrix<T, 3, 3>;
  using Vector = Eigen::Matrix<T, 3, 1>;

  Matrix Ra = Ta.rotation(), Rb = Tb.rotation();
  for (int i = 0; i < Ra.rows(); ++i)
  {
    for (int j = 0; j < Ra.cols(); ++j)
    {
      EXPECT_NEAR(Ra(i, j), Rb(i, j), TOLERANCE);
    }
  }

  Vector pa = Ta.translation(), pb = Tb.translation();
  EXPECT_NEAR(pa[0], pb[0], TOLERANCE);
  EXPECT_NEAR(pa[1], pb[1], TOLERANCE);
  EXPECT_NEAR(pa[2], pb[2], TOLERANCE);
}

TEST(testPlugin, testInit)
{
  moveit_opw_kinematics_plugin::MoveItOPWKinematicsPlugin plugin;
  bool res = plugin.initialize("robot_description", "manipulator", "base_link", "tool0", 0.1);
  EXPECT_TRUE(res);
}

class LoadPlugin : public testing::Test
{
protected:
  void SetUp() override
  {
    plugin_.initialize("robot_description", "manipulator", "base_link", "tool0", 0.1);
  };
  void TearDown() override
  {
  };
protected:
  moveit_opw_kinematics_plugin::MoveItOPWKinematicsPlugin plugin_;
};

TEST_F(LoadPlugin, positionFK)
{
  using Eigen::AngleAxisd;
  using Eigen::Translation3d;
  using Eigen::Vector3d;

  std::vector<std::string> link_names;
  std::vector<double> joint_angles = {0, 0, 0, 0, 0, 0};
  std::vector<geometry_msgs::Pose> poses;

  plugin_.getPositionFK(plugin_.getLinkNames(), joint_angles, poses);

  Eigen::Affine3d pose_actual, pose_desired;
  tf::poseMsgToEigen(poses[0], pose_actual);

  // position and orientation taking into account the offset of the second joint
  // px = a1 + c2 + c3 + c4
  // py = 0
  // pz = c1 + a2
  pose_desired = Translation3d(0.785, 0, 0.435) * AngleAxisd(M_PI_2, Vector3d::UnitY());

  comparePoses(pose_actual, pose_desired);

}

TEST_F(LoadPlugin, singleSolutionIK)
{
  const std::vector<double> joint_angles = {0, 0.1, 0.2, 0.3, 0.4, 0.5};
  std::vector<geometry_msgs::Pose> poses;

  plugin_.getPositionFK(plugin_.getLinkNames(), joint_angles, poses);
  const geometry_msgs::Pose pose_in = poses[0];

  std::vector<double> solution;
  moveit_msgs::MoveItErrorCodes error_code;
  bool res = plugin_.getPositionIK(pose_in, joint_angles, solution, error_code);
  EXPECT_TRUE(res);

  for (int i = 0; i < solution.size(); ++i)
  {
    EXPECT_NEAR(solution[i], joint_angles[i], TOLERANCE);
  }
}

TEST_F(LoadPlugin, allSolutionsIK)
{
  std::vector<std::string> link_names;
  const std::vector<double> joint_angles = {0, 0.1, 0.2, 0.3, 0.4, 0.5};
  std::vector<geometry_msgs::Pose> poses_out;

  // find reachable pose
  plugin_.getPositionFK(plugin_.getLinkNames(), joint_angles, poses_out);

  // calculate all ik solutions for this pose
  const std::vector<geometry_msgs::Pose> poses_in = {poses_out[0]};
  std::vector<std::vector<double> > solutions;
  kinematics::KinematicsResult result;
  bool res = plugin_.getPositionIK(poses_in, joint_angles, solutions, result);
  EXPECT_TRUE(res);

  // check if fk for all this solutions gives the same pose
  Eigen::Affine3d actual, desired;
  tf::poseMsgToEigen(poses_out[0], desired);
  for (auto js : solutions)
  {
    plugin_.getPositionFK(plugin_.getLinkNames(), js, poses_out);
    tf::poseMsgToEigen(poses_out[0], actual);
    comparePoses(actual, desired);

  }
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "test_moveit_opw_kinematics_plugin");
    testing::InitGoogleTest(&argc, argv);
    bool res = RUN_ALL_TESTS();
    ros::shutdown();
    return res;
}