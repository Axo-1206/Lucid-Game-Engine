#include <catch2/catch_test_macros.hpp>

#include "engine/registry/operation.h"

using namespace engine;

TEST_CASE("operation: named operations resolve to a name and a group",
          "[registry][operation]")
{
    REQUIRE(std::string(operation_name(Operation::Add)) == "Add");
    REQUIRE(std::string(operation_group(Operation::Add)) == "Arithmetic");

    REQUIRE(std::string(operation_name(Operation::Sub)) == "Sub");
    REQUIRE(std::string(operation_group(Operation::Sub)) == "Arithmetic");

    REQUIRE(std::string(operation_name(Operation::LessThan)) == "LessThan");
    REQUIRE(std::string(operation_group(Operation::LessThan)) == "Comparison");

    REQUIRE(std::string(operation_name(Operation::If)) == "If");
    REQUIRE(std::string(operation_group(Operation::If)) == "Flow");

    REQUIRE(std::string(operation_name(Operation::Print)) == "Print");
    REQUIRE(std::string(operation_group(Operation::Print)) == "Debug");

    REQUIRE(std::string(operation_name(Operation::IntToFloat)) == "IntToFloat");
    REQUIRE(std::string(operation_group(Operation::IntToFloat)) == "Conversion");
}

TEST_CASE("operation: None and Custom resolve to empty strings",
          "[registry][operation]")
{
    REQUIRE(std::string(operation_name(Operation::None)) == "");
    REQUIRE(std::string(operation_group(Operation::None)) == "");

    REQUIRE(std::string(operation_name(Operation::Custom)) == "");
    REQUIRE(std::string(operation_group(Operation::Custom)) == "");
}