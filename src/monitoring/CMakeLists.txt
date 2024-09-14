aux_source_directory(src MONITORING_SRC)

if (CCT_ENABLE_PROMETHEUS)
  list(REMOVE_ITEM MONITORING_SRC "src/voidmetricgateway.cpp")
else()
  list(REMOVE_ITEM MONITORING_SRC "src/prometheusmetricgateway.cpp")
endif()

add_coincenter_library(monitoring STATIC ${MONITORING_SRC})
target_link_libraries(coincenter_monitoring PRIVATE spdlog)
target_link_libraries(coincenter_monitoring PUBLIC coincenter_tech)

if (CCT_ENABLE_PROMETHEUS)
  target_link_libraries(coincenter_monitoring PUBLIC prometheus-cpp::push)
endif()

