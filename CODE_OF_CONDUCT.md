Linters:

For Python code:
Use flake8 --max-line-length=179 .

For C++ code:
use clang-tidy -checks='readability-braces-around-statements,readability-container-size-empty,readability-else-after-return,readability-function-size,readability-identifier-naming,readability-implicit-bool-cast,readability-inconsistent-declaration-parameter-name,readability-named-parameter,readability-redundant-smartptr-get,readability-redundant-string-cstr,readability-uniqueptr-delete-release,google-readability-braces-around-statements,google-readability-casting,google-readability-function-size,google-readability-namespace-comments,google-readability-redundant-smartptr-get,google-readability-todo'
and
clang-format -style=LLVM