#pragma once

#define ArraySize(array) (sizeof(array) / sizeof(array[0]))

template<typename T, typename... Args>
bool is_one_of(T var, Args... args)
{
  return ((var == args) || ...);
}