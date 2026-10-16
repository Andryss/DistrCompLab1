#include "test_support.h"
#include "test_cases.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Отличает исходные файлы от объектов, исполняемых файлов и логов. */
static int is_source(const char* name) {
    const char* extension = strrchr(name, '.');
    return extension != NULL && (!strcmp(extension, ".c") || !strcmp(extension, ".h"));
}

/** Собирает исходники из модулей и проверяет уникальность имён для плоского архива. */
static void collect_sources(const char* directory_path, char sources[64][PATH_MAX], int* count) {
    DIR* directory = opendir(directory_path);
    assert_non_null(directory);
    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char path[PATH_MAX];
        test_path(path, directory_path, entry->d_name);
        struct stat info;
        assert_int_equal(0, stat(path, &info));
        if (S_ISDIR(info.st_mode)) {
            collect_sources(path, sources, count);
        } else if (is_source(entry->d_name)) {
            assert_true(S_ISREG(info.st_mode));
            assert_true(*count < 64);
            for (int index = 0; index < *count; ++index) {
                assert_true(strcmp(strrchr(sources[index], '/') + 1, entry->d_name) != 0);
            }
            size_t length = strlen(path);
            assert_true(length < sizeof(sources[*count]));
            memcpy(sources[*count], path, length + 1);
            ++*count;
        }
    }
    assert_int_equal(0, closedir(directory));
}

/** Побайтово сравнивает оригинал и его копию из сдаваемого архива. */
static void compare_files(const char* original, const char* extracted) {
    FILE* first = fopen(original, "rb");
    FILE* second = fopen(extracted, "rb");
    assert_non_null(first);
    assert_non_null(second);
    int a, b;
    do {
        a = fgetc(first);
        b = fgetc(second);
        assert_true(a == b);
    } while (a != EOF);
    assert_true(!ferror(first) && !ferror(second));
    assert_true(fclose(first) == 0 && fclose(second) == 0);
}

/** Проверяет, что каждый элемент архива — исходник верхнего уровня каталога приложения. */
static int check_archive_entries(const char* archive, char sources[64][PATH_MAX], int source_count, const char* work) {
    char* arguments[] = {"tar", "-tzf", (char*)archive, NULL};
    assert_true(test_command(arguments, work, 10) == 0);
    char* contents = test_stdout();
    int count = 0;
    char* context = NULL;
    for (char* entry = strtok_r(contents, "\n", &context); entry != NULL;
         entry = strtok_r(NULL, "\n", &context)) {
        if (!strcmp(entry, TEST_APP_NAME "/")) {
            continue;
        }
        assert_true(!strncmp(entry, TEST_APP_NAME "/", strlen(TEST_APP_NAME) + 1));
        const char* name = entry + strlen(TEST_APP_NAME) + 1;
        assert_null(strchr(name, '/'));
        if (!strcmp(name, "libruntime.so")) {
            continue;
        }
        assert_true(is_source(name));
        int found = 0;
        for (int index = 0; index < source_count; ++index) {
            if (!strcmp(name, strrchr(sources[index], '/') + 1)) {
                found = 1;
                break;
            }
        }
        assert_true(found);
        ++count;
    }
    free(contents);
    return count;
}

/** Собирает извлечённые исходники командой задания и запускает на них весь протокол. */
static void compile_extracted_sources(char sources[64][PATH_MAX], int source_count, const char* extracted, int original_runtime) {
    char* arguments[64] = {"clang", "-std=c99", "-Wall", "-pedantic", NULL};
    int argument_count = 4;
    for (int index = 0; index < source_count; ++index) {
        const char* name = strrchr(sources[index], '/') + 1;
        char copy[PATH_MAX];
        test_path(copy, extracted, name);
        compare_files(sources[index], copy);
        if (!strcmp(strrchr(name, '.'), ".c")) {
            assert_true(argument_count < 61);
            arguments[argument_count++] = (char*)name;
        }
    }
    if (original_runtime) {
        arguments[argument_count++] = "-L.";
        arguments[argument_count++] = "-lruntime";
    } else {
        arguments[argument_count++] = "-I.";
        arguments[argument_count++] = TEST_RUNTIME_SOURCE;
    }
    arguments[argument_count] = NULL;
    assert_true(test_command(arguments, extracted, 60) == 0);
    char* errors = test_stderr();
    if (errors[0] != '\0') {
        fputs(errors, stderr);
    }
    assert_true(errors[0] == '\0');
    free(errors);

}

/** Запускает проверки с путём к библиотеке и восстанавливает окружение тестов. */
static void run_extracted_program(const char* extracted, int original_runtime) {
    char binary[PATH_MAX];
    test_path(binary, extracted, "a.out");
    const char* previous_path = getenv("LD_LIBRARY_PATH");
    char* saved_path = previous_path == NULL ? NULL : strdup(previous_path);
    if (previous_path != NULL) {
        assert_non_null(saved_path);
    }
    if (original_runtime) {
        assert_int_equal(0, setenv("LD_LIBRARY_PATH", extracted, 1));
    }
    if (original_runtime) {
        test_program_original(binary);
    } else {
        test_program(binary);
    }
    if (saved_path != NULL) {
        assert_int_equal(0, setenv("LD_LIBRARY_PATH", saved_path, 1));
        free(saved_path);
    } else {
        assert_int_equal(0, unsetenv("LD_LIBRARY_PATH"));
    }
}

void test_package(const char* repository, int original_runtime) {
    char path[PATH_MAX];
    test_path(path, TEST_BUILD_DIRECTORY, "dist");
    assert_true(mkdir(path, 0700) == 0 || errno == EEXIST);
    test_path(path, TEST_BUILD_DIRECTORY, "dist/" TEST_APP_NAME);
    assert_true(mkdir(path, 0700) == 0 || errno == EEXIST);
    test_path(path, TEST_BUILD_DIRECTORY, "dist/" TEST_APP_NAME "/a.out");
    FILE* stale = fopen(path, "w");
    assert_non_null(stale);
    assert_true(fputs("old build artifact", stale) >= 0);
    assert_true(fclose(stale) == 0);

    char* runtime[] = {"make", "runtime", "BUILD_DIR=" TEST_BUILD_DIRECTORY, NULL};
    assert_int_equal(0, test_command(runtime, repository, 60));
    char* package[] = {"make", "package", "BUILD_DIR=" TEST_BUILD_DIRECTORY,
        "APP_NAME=" TEST_APP_NAME, "RUNTIME_LIB=" TEST_RUNTIME_LIBRARY, NULL};
    assert_true(test_command(package, repository, 60) == 0);
    char archive[PATH_MAX], sources[PATH_MAX], work[PATH_MAX], extracted[PATH_MAX];
    test_path(archive, TEST_BUILD_DIRECTORY, "dist/" TEST_APP_NAME ".tar.gz");
    test_path(sources, repository, "src");
    test_case_directory(work);
    char source_files[64][PATH_MAX];
    int source_count = 0;
    collect_sources(sources, source_files, &source_count);
    assert_int_equal(source_count, check_archive_entries(archive, source_files, source_count, work));
    char* extract[] = {"tar", "-xzf", archive, NULL};
    assert_true(test_command(extract, work, 10) == 0);
    test_path(extracted, work, TEST_APP_NAME);
    char copy[PATH_MAX];
    test_path(copy, extracted, "libruntime.so");
    compare_files(TEST_RUNTIME_LIBRARY, copy);
    compile_extracted_sources(source_files, source_count, extracted, original_runtime);
    run_extracted_program(extracted, original_runtime);
    puts("Package tests passed");
}
