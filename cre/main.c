#include <stdio.h>
#include <cre/cre.h>

void usage(const char * name) {
    printf(
        "Usage: %s [OPTION]... REGEX\n"
        "Command line interface to the cre regex library.\n"
        "\n"
        "Arguments\n"
        "  -d, --dump-regex            print the compiled regex datastructure.\n"
        "\n",
        name
    );
}

int main(int argc, char** argv) {
    if (argc == 1) {
        printf("error: must supply a regex\n");
        usage(argv[0]);
        return 0;
    }

    printf("Regex: %s\n", argv[1]);
    cre_re_t * re = cre_compile(argv[1]);
    cre_print(re);
    cre_free(re);

    return 0;
}