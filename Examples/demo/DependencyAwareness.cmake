DAP_INFO(
  NAME demo
  VERSION 0.1.0
)

DAP(
  NAME sub-a
  REQUIRE ">= 0.1.1"
  LOCATION "${DEMO_DIR}/sub-a#v0.1.1"
)
DAP(
  NAME sub-b
  REQUIRE ">= 0.1.2"
  LOCATION "${DEMO_DIR}/sub-b#v0.1.2"
)
