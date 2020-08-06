#include "file_info.h"
#include <locale.h>

static const char CURRENT_DIRECTORY[] = ".";


int get_number_count(int number){
    int number_count = 0;
    while (number != 0){
        number_count++;
        number/=10;
    }
    return number_count;
}


long get_total(struct file_data* files, int file_count) {
    long total = 0;

    for (int i = 0; i < file_count; ++i) {
        total+=files[i].info.st_blocks;
    }

    return total / 2;
}

struct align_parameters get_align_parameters(struct file_data* files, int files_count)
{
    int hard_links_align = 0;
    int user_align = 0;
    int group_align = 0;
    int byte_size_align = 0;
    int mod_time_size_align = 0;
    struct align_parameters align;

    for (int i = 0; i < files_count; ++i) {
        if (hard_links_align < get_number_count(files[i].info.st_nlink)){
            hard_links_align = get_number_count(files[i].info.st_nlink);
        }

        if (user_align < strlen(files[i].user_string)){
            user_align = strlen(files[i].user_string);
        }

        if (group_align < strlen(files[i].user_string)){
            group_align = strlen(files[i].user_string);
        }
        if (byte_size_align < get_number_count(files[i].info.st_size)){
            byte_size_align = get_number_count(files[i].info.st_size);
        }

        if (mod_time_size_align < strlen(files[i].mod_time_str)){
            mod_time_size_align = strlen(files[i].mod_time_str);
        }
    }

    align.byte_size_align = byte_size_align;
    align.user_align = user_align;
    align.group_align = group_align;
    align.hard_links_align = hard_links_align;
    align.mod_time_size_align = mod_time_size_align;

    return align;
}


void print_directory_files(const char* dir_path)
{
    DIR* dir;
    struct dirent* ent;
    struct stat file_info;
    struct file_data* dir_files = NULL;
    char* full_path;
    int files_count = 0;
    int status;
    char* link_path;
    char* real_name;
    struct align_parameters align;

    dir = opendir(dir_path);
    if (dir == NULL) {
        perror("");
        return;
    }

    while ((ent = readdir(dir)) != NULL){
        if (is_file_hidden(ent->d_name)){
            continue;
        }
        full_path = calloc(strlen(dir_path) + strlen(ent->d_name) + 2, sizeof(char));
        sprintf(full_path, "%s/%s", dir_path, ent->d_name);
        status = lstat(full_path, &file_info);
        if (status != 0){
            perror("");
            continue;
        }

        files_count++;
        dir_files = realloc(dir_files, sizeof(*dir_files) * files_count);
        dir_files[files_count - 1] = get_file_info(file_info, full_path);
    }
    closedir(dir);

    qsort(dir_files, files_count, sizeof(*dir_files), sort_files);
    align = get_align_parameters(dir_files, files_count);
    printf("total %ld\n", get_total(dir_files, files_count));
    for (int i = 0; i < files_count; ++i) {
        //TODO
        real_name = dir_files[i].name;
        dir_files[i].name = get_file_name(dir_files[i].name);
        print_file_info(dir_files[i], align);
        if (S_ISLNK(dir_files[i].info.st_mode)){
            link_path = get_symlink_path(real_name);
            if (link_path != NULL){
                printf(" -> %s\n", link_path);
                free(link_path);
            }
        } else {
            printf("\n");
        }
        dir_files[i].name = real_name;
        if (real_name != NULL){
            free(real_name);
            dir_files[i].name = NULL;
            free_file_info(&dir_files[i]);
        }
    }

    free(dir_files);
}


void parse_arguments(int argc, char** argv, struct file_data** parsed_args, int* files_count)
{
    struct file_data* args_buffer;
    int status;
    int args_count = 0;
    struct stat file_stat;

    *files_count = argc == 1 ? 1 : argc - 1;
    args_buffer = calloc(*files_count, sizeof(struct file_data));
    if (args_buffer == NULL){
        *files_count = 0;
        *parsed_args = NULL;
        return;
    }

    if (argc == 1){
        status = lstat(CURRENT_DIRECTORY, &file_stat);
        if (status != 0) {
            perror("");
            free(args_buffer);
            *parsed_args = NULL;
            *files_count = 0;
            return;
        }
        args_buffer[0] = get_file_info(file_stat, CURRENT_DIRECTORY);
        args_count++;
    } else {
        for (int i = 0; i < *files_count; ++i) {
            status = lstat(argv[i + 1], &file_stat);
            if (status != 0){
                perror("");
                continue;
            }
            args_buffer[i] = get_file_info(file_stat, argv[i + 1]);
            args_count++;
        }
    }

    qsort(args_buffer, args_count, sizeof(*args_buffer), sort_files);

    *parsed_args = args_buffer;
    *files_count = args_count;
}


int main(int argc, char** argv) {
    struct file_data* files;
    int args_count;
    struct align_parameters align;

    setlocale(LC_ALL, "");
    setlocale(LC_COLLATE, "");

    parse_arguments(argc, argv, &files, &args_count);

    if (args_count == 0){
        return 1;
    }

    if (args_count == 1){
        print_directory_files(files[0].name);
    } else {
        align = get_align_parameters(files, args_count);
        for (int i = 0; i < args_count; ++i) {
            if (S_ISDIR(files[i].info.st_mode)){
                continue;
            }
            print_file_info(files[i], align);
            printf("\n");
        }

        for (int j = 0; j < args_count; ++j) {
            if (!S_ISDIR(files[j].info.st_mode)){
                free_file_info(&files[j]);
                continue;
            }
            printf("\n%s:\n", files[j].name);
            print_directory_files(files[j].name);
            free_file_info(&files[j]);
        }
    }

    free(files);

    return 0;
}
