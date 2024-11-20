#include <compiler.hpp>
#include <iostream>
#include <string>
#include <utils.hpp>
#include <vector>

int compiler::Compiler::Compile(std::string SourceFile) {
  std::string Options;
  for (auto option = this->CompilerOptions.begin();
       option != this->CompilerOptions.end(); option++) {
    Options += option->data();
    Options += " ";
  }
  std::string FinalCommand = this->CompilerName + " " + Options + "-c " +
                             SourceFile + " -o " + this->BuildFolder + "/" +
                             utils::GetFilenameWithoutExtension(SourceFile) +
                             ".o";
  std::cout << FinalCommand << std::endl;
  return utils::ExecuteCommand(FinalCommand);
}

int compiler::Compiler::Link(std::vector<std::string> ObjectFiles) {
  std::string Objects;
  for (auto object = ObjectFiles.begin(); object != ObjectFiles.end();
       object++) {
    Objects += object->data();
    Objects += " ";
  }
  Objects.pop_back();
  std::string FinalCommand = this->CompilerName + " " + Objects + " -o " +
                             this->BuildFolder + "/" + this->Target;
  std::cout << FinalCommand << std::endl;
  return utils::ExecuteCommand(FinalCommand);
}