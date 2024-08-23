function (DEMO_HANDLE_DEMO -out)
  cmake_parse_arguments (PARSE_ARGV 1 -arg "" "HOST;USER;PATH" "")
  set ("${-out}" "${DEMO_DIR}/${-arg_PATH}" PARENT_SCOPE)
endfunction ()

DAPPER_REGISTER_HOST(demo DEMO_HANDLE_DEMO)
