#include <thirdparty/catch2/catch_amalgamated.hpp>
#include <phallocators/Debug.hpp>

int main( int argc, char* argv[] ) {
  Debug::Initialize(stdout, true);
  int result = Catch::Session().run( argc, argv );
  return result;
}