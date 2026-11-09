add_library(gsl INTERFACE)
add_library(Microsoft.GSL::GSL ALIAS gsl)
target_include_directories(gsl SYSTEM INTERFACE ./include/)
