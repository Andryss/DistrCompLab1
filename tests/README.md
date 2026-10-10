```sh
# Зависимости тестов — macOS
brew install cmocka pkgconf
brew install llvm  # clang-tidy
export PATH="$(brew --prefix llvm)/bin:$PATH"

# Зависимости — Ubuntu / Debian
sudo apt install clang clang-tidy make libcmocka-dev pkg-config

# Сборка и тесты — из корня репозитория
make                  # программа build/pa1 без санитайзеров
make tests            # программа и тесты с ASan/UBSan в build/sanitizers
make test             # сборка и запуск тестов с ASan/UBSan
make lint             # clang-tidy для src и tests
make package          # архив build/dist/pa1.tar.gz
make clean            # удаление каталога build
```
