#include "app/ProgramEntry.hpp"

#ifdef _WIN32
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  return cliphist::RunProgram(0, nullptr, false);
}
#else
int main(int argc, char** argv) {
  return cliphist::RunProgram(argc, argv, false);
}
#endif
