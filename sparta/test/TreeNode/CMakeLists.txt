project(Treenode_test)

sparta_add_test_executable(TreeNode_test TreeNode_test.cpp)

sparta_test(TreeNode_test TreeNode_test_RUN)
sparta_copy(TreeNode_test *.json)
