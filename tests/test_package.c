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
            strcpy(sources[(*count)++], path);
        }
    }
    assert_int_equal(0, closedir(directory));
}

/** Побайтово сравнивает оригинал и его копию из сдаваемого архива. */
static void compare_files(const char* original, const char* extracted) {
    FILE* first = fopen(original, "rb");
    FILE* second = fopen(extracted, "rb");
    assert_true(first != NULL && second != NULL);
    int a, b;
    do {
        a = fgetc(first);
        b = fgetc(second);
        assert_true(a == b);
    } while (a != EOF);
    assert_true(!ferror(first) && !ferror(second));
    assert_true(fclose(first) == 0 && fclose(second) == 0);
}

/** Проверяет, что каждый элемент архива — исходник верхнего уровня каталога pa1. */
static int check_archive_entries(const char* archive, char sources[64][PATH_MAX], int source_count, const char* work) {
    char* arguments[] = {"tar", "-tzf", (char*)archive, NULL};
    assert_true(test_command(arguments, work, 10) == 0);
    char* contents = test_stdout();
    int count = 0;
    char* context = NULL;
    for (char* entry = strtok_r(contents, "\n", &context); entry != NULL;
         entry = strtok_r(NULL, "\n", &context)) {
        if (!strcmp(entry, "pa1/")) {
            continue;
        }
        assert_true(!strncmp(entry, "pa1/", 4));
        const char* name = entry + 4;
        assert_true(strchr(name, '/') == NULL && is_source(name));
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
static void compile_and_run(char sources[64][PATH_MAX], int source_count, const char* extracted) {
    char* arguments[64] = {"clang", "-std=c99", "-Wall", "-pedantic", NULL};
    int argument_count = 4;
    for (int index = 0; index < source_count; ++index) {
        const char* name = strrchr(sources[index], '/') + 1;
        char copy[PATH_MAX];
        test_path(copy, extracted, name);
        compare_files(sources[index], copy);
        if (!strcmp(strrchr(name, '.'), ".c")) {
            assert_true(argument_count < 63);
            arguments[argument_count++] = (char*)name;
        }
    }
    arguments[argument_count] = NULL;
    assert_true(test_command(arguments, extracted, 60) == 0);
    char* errors = test_stderr();
    if (errors[0] != '\0') {
        fputs(errors, stderr);
    }
    assert_true(errors[0] == '\0');
    free(errors);

    char binary[PATH_MAX];
    test_path(binary, extracted, "a.out");
    test_program(binary);
}

void test_package(const char* repository) {
    char path[PATH_MAX];
    test_path(path, PA1_BUILD_DIRECTORY, "dist");
    assert_true(mkdir(path, 0700) == 0 || errno == EEXIST);
    test_path(path, PA1_BUILD_DIRECTORY, "dist/pa1");
    assert_true(mkdir(path, 0700) == 0 || errno == EEXIST);
    test_path(path, PA1_BUILD_DIRECTORY, "dist/pa1/a.out");
    FILE* stale = fopen(path, "w");
    assert_true(stale != NULL);
    assert_true(fputs("old build artifact", stale) >= 0);
    assert_true(fclose(stale) == 0);

    char* package[] = {"make", "package", "BUILD_DIR=" PA1_BUILD_DIRECTORY, NULL};
    assert_true(test_command(package, repository, 60) == 0);
    char archive[PATH_MAX], sources[PATH_MAX], work[PATH_MAX], extracted[PATH_MAX];
    test_path(archive, PA1_BUILD_DIRECTORY, "dist/pa1.tar.gz");
    test_path(sources, repository, "src");
    test_case_directory(work);
    char source_files[64][PATH_MAX];
    int source_count = 0;
    collect_sources(sources, source_files, &source_count);
    assert_int_equal(source_count, check_archive_entries(archive, source_files, source_count, work));
    char* extract[] = {"tar", "-xzf", archive, NULL};
    assert_true(test_command(extract, work, 10) == 0);
    test_path(extracted, work, "pa1");
    compile_and_run(source_files, source_count, extracted);
    puts("Package tests passed");
}
