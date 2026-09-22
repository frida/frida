add_custom_command(
  OUTPUT cpyTest4.hpp
  COMMAND mycpy "${CMAKE_CURRENT_SOURCE_DIR}/cpyTest4.hpp" cpyTest4.hpp
  DEPENDS cpyTest4.hpp
)

add_custom_target(tgtCpyTest4 DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/cpyTest4.hpp")
