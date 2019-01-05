#pragma once

#ifdef _DEBUG
constexpr auto CompileConfiguration = "Debug";
#else
constexpr auto CompileConfiguration = "Release";
#endif