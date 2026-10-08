```sh
# Зависимости тестов — macOS
brew install cmocka pkgconf

# Зависимости — Ubuntu / Debian
sudo apt install clang make libcmocka-dev pkg-config

# Сборка и тесты — из корня репозитория
make                  # программа build/pa1
make tests            # сборка программы и build/pa1_tests
make test             # сборка и запуск тестов
make SANITIZE=1 test  # тесты с ASan/UBSan в build/sanitizers
make package          # архив build/dist/pa1.tar.gz
make clean            # удаление каталога build
```
