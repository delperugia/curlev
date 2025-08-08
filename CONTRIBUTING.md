# Contributing to Curlev

Thank you for your interest in contributing! Please follow these guidelines to help us maintain a high-quality codebase.

## How to Contribute

1. **Fork the repository** and create your branch from `main`.
2. **Describe your changes** clearly in your pull request.

## Code Formatting

- Follow the project's naming conventions.
- Use `clang-format` to format your code, prioritizing clarity and ease of reading.
- Use consistent indentation (2 spaces).
- Remove unused code and imports.
- Run linters before submitting (`cmake` targets `cppcheck` and `clang-tidy`).
- If needed, update the documentation.

## Adding Tests

- **All new features must include tests.**
- Place tests in the appropriate `tests/` directory.
- Use descriptive test names.
- Ensure all tests pass (using `ctest`) before submitting your PR.

## Pull Request Checklist

- [ ] Code is formatted and linted.
- [ ] New features include tests.
- [ ] All tests pass.
- [ ] Documentation is up to date.
- [ ] PR description explains the purpose and changes.

Thank you for helping improve Curlev!
