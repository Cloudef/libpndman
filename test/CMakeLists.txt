SET(TEST_EXE
   crawl
   device
   handle
   list
   pxml
   repo
   repo_api
   sample
   update)

FIND_PACKAGE(Threads)
IF (CMAKE_USE_PTHREADS_INIT OR CMAKE_HP_PTHREADS_INIT)
   LIST(APPEND TEST_EXE pthread)
ENDIF ()

FOREACH (test ${TEST_EXE})
   PROJECT(${test})
   ADD_EXECUTABLE(${test} ${test}.c)
   INCLUDE_DIRECTORIES(${libpndman_SOURCE_DIR}/include)
   TARGET_LINK_LIBRARIES(${test} pndman ${LIBPNDMAN_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDFOREACH (test)

