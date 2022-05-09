///
/// @file  compiler_workaround.hpp
/// @brief Workaround for the non-standard behaviors of the compiler.
///

#ifndef SYNC_CELL_COMPILER_WORKAROUND_HPP
#define SYNC_CELL_COMPILER_WORKAROUND_HPP

// Call a template method on a dependent name.
// Inside a template definition, `template` can be used to declare that a dependent name
// is a template. See https://en.cppreference.com/w/cpp/language/dependent_name.
// However, MSVC will report an error if we add the 'template' keyword correctly.
#if defined(_MSC_VER)
#define TEMPLATE_CALL
#else
#define TEMPLATE_CALL template
#endif

#endif //SYNC_CELL_COMPILER_WORKAROUND_HPP
