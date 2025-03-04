#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

/* Path for the log file */
#define LOGFILE "/mnt/us/extensions/kindle_browser_patch/kindle_browser_patch.log"

/* Logging function: appends a timestamped message to the log file */
void log_message(const char *format, ...) {
    FILE *logf = fopen(LOGFILE, "a");
    if (!logf) {
        fprintf(stderr, "Error opening log file %s\n", LOGFILE);
        return;
    }
    time_t now = time(NULL);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(logf, "[%s] ", timestr);

    va_list args;
    va_start(args, format);
    vfprintf(logf, format, args);
	vprintf(format, args);
	printf("\n");
    va_end(args);
    fprintf(logf, "\n");
    fclose(logf);
}

/* Runs a command using system() and logs its execution.
   Returns 0 on success, -1 on failure. */
int run_command(const char *cmd) {
    log_message("Running command: %s", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        log_message("Command failed: %s (exit code %d)", cmd, ret);
        return -1;
    }
    log_message("Command succeeded: %s", cmd);
    return 0;
}

/* Runs a command and captures its output.
   The output (first line) is stored in the provided buffer.
   Returns 0 on success, -1 on failure. */
int run_command_capture(const char *cmd, char *output, size_t output_size) {
    log_message("Running command (capture): %s", cmd);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        log_message("Failed to run command: %s", cmd);
        return -1;
    }
    if (fgets(output, output_size, fp) == NULL) {
        log_message("No output captured from command: %s", cmd);
        pclose(fp);
        return -1;
    }
    int ret = pclose(fp);
    if (ret != 0) {
        log_message("Command (capture) failed: %s (exit code %d)", cmd, ret);
        return -1;
    }
    /* Remove trailing newline if present */
    size_t len = strlen(output);
    if (len > 0 && output[len-1] == '\n') {
        output[len-1] = '\0';
    }
    log_message("Command (capture) succeeded: %s, output: %s", cmd, output);
    return 0;
}

/* Applies a binary patch to a file.
   It opens the file in binary mode, searches for the pattern, and if found exactly once,
   writes the replacement bytes. Returns 0 on success, -1 on failure. */
int apply_patch(const char *filepath, const unsigned char *find, const unsigned char *replace, size_t len) {
    log_message("Applying patch on file %s", filepath);
    FILE *f = fopen(filepath, "rb+");
    if (!f) {
        log_message("Failed to open file %s: %s", filepath, strerror(errno));
        return -1;
    }

    /* Determine file size */
    if (fseek(f, 0, SEEK_END) != 0) {
        log_message("Failed to seek in file %s", filepath);
        fclose(f);
        return -1;
    }
    long filesize = ftell(f);
    if (filesize < 0) {
        log_message("Failed to get file size for %s", filepath);
        fclose(f);
        return -1;
    }
    rewind(f);

    unsigned char *buffer = malloc(filesize);
    if (!buffer) {
        log_message("Failed to allocate memory for file %s", filepath);
        fclose(f);
        return -1;
    }
    if (fread(buffer, 1, filesize, f) != (size_t)filesize) {
        log_message("Failed to read file %s", filepath);
        free(buffer);
        fclose(f);
        return -1;
    }

    /* Search for pattern occurrences */
    int count = 0;
    long pos = -1;
    for (long i = 0; i <= filesize - (long)len; i++) {
        if (memcmp(buffer + i, find, len) == 0) {
            count++;
            pos = i;
        }
    }
    if (count != 1) {
        log_message("Pattern found %d times in file %s (expected exactly 1)", count, filepath);
        free(buffer);
        fclose(f);
        return -1;
    }

    /* Seek to the found position and write the replacement bytes */
    if (fseek(f, pos, SEEK_SET) != 0) {
        log_message("Failed to seek in file %s: %s", filepath, strerror(errno));
        free(buffer);
        fclose(f);
        return -1;
    }
    if (fwrite(replace, 1, len, f) != len) {
        log_message("Failed to write patch to file %s: %s", filepath, strerror(errno));
        free(buffer);
        fclose(f);
        return -1;
    }
    log_message("Patch applied successfully at position %ld in file %s", pos, filepath);
    free(buffer);
    fclose(f);
    return 0;
}

/* Prints usage information to stderr */
void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s [install|uninstall]\n", progname);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "install") == 0) {
        log_message("Starting installation process");

        /* 1. Delete everything in the folder /mnt/us/extensions/kindle_browser_patch/patched_bin (delete and re-create) */
        if (run_command("rm -rf /mnt/us/extensions/kindle_browser_patch/patched_bin") != 0) {
            log_message("Failed to remove /mnt/us/extensions/kindle_browser_patch/patched_bin");
            return EXIT_FAILURE;
        }
        if (mkdir("/mnt/us/extensions/kindle_browser_patch/patched_bin", 0755) != 0) {
            log_message("Failed to create /mnt/us/extensions/kindle_browser_patch/patched_bin: %s", strerror(errno));
            return EXIT_FAILURE;
        }
        log_message("Cleared and recreated /mnt/us/extensions/kindle_browser_patch/patched_bin");

        /* 2. Copy /usr/bin/browser to /mnt/us/extensions/kindle_browser_patch/patched_bin */
        if (run_command("cp /usr/bin/browser /mnt/us/extensions/kindle_browser_patch/patched_bin/") != 0) {
            log_message("Failed to copy /usr/bin/browser to /mnt/us/extensions/kindle_browser_patch/patched_bin");
            return EXIT_FAILURE;
        }

        /* 3. Copy /usr/bin/chromium folder to /mnt/us/extensions/kindle_browser_patch/patched_bin */
        if (run_command("cp -r /usr/bin/chromium /mnt/us/extensions/kindle_browser_patch/patched_bin/") != 0) {
            log_message("Failed to copy /usr/bin/chromium to /mnt/us/extensions/kindle_browser_patch/patched_bin");
            return EXIT_FAILURE;
        }

        /* 4. Apply binary patches */
        const char *kindle_browser_path = "/mnt/us/extensions/kindle_browser_patch/patched_bin/chromium/bin/kindle_browser";
        {
            unsigned char patch1_find[]    = { 0x0C, 0X36, 0X0C, 0X35, 0X00, 0X28, 0XE8, 0XD0, 0X01, 0X25, 0X00, 0XE0, 0X00, 0X25 };
            unsigned char patch1_replace[] = { 0x0C, 0X36, 0X0C, 0X35, 0X00, 0X28, 0XE8, 0XD0, 0X01, 0X25, 0X00, 0XE0, 0X01, 0X25 };
			
			if (sizeof(patch1_find) != sizeof(patch1_replace)) {
				log_message("Failed to apply patch, patch_find and patch_replace must be the same size");
				return EXIT_FAILURE;
			}
			
            if (apply_patch(kindle_browser_path, patch1_find, patch1_replace, sizeof(patch1_find)) != 0) {
                log_message("Failed to apply first binary patch");
                return EXIT_FAILURE;
            }
        }
		const char *libchromium_path = "/mnt/us/extensions/kindle_browser_patch/patched_bin/chromium/bin/libchromium.so";
        {
            unsigned char patch2_find[]    = { 0X0D, 0X48, 0X78, 0X44, 0X05, 0X68, 0X28, 0X46, 0xA5, 0XF0, 0X87, 0XF9, 0X02, 0X46, 0X20, 0X46, 0X29, 0X46, 0XFF, 0XF7, 0XDA, 0XFF, 0X08, 0XB1 };
            unsigned char patch2_replace[] = { 0X0D, 0X48, 0X78, 0X44, 0X05, 0X68, 0X28, 0X46, 0xA5, 0XF0, 0X87, 0XF9, 0X02, 0X46, 0X20, 0X46, 0X29, 0X46, 0XFF, 0XF7, 0XDA, 0XFF, 0X00, 0XBF };
			
			if (sizeof(patch2_find) != sizeof(patch2_replace)) {
				log_message("Failed to apply patch, patch_find and patch_replace must be the same size");
				return EXIT_FAILURE;
			}
			
            if (apply_patch(libchromium_path, patch2_find, patch2_replace, sizeof(patch2_find)) != 0) {
                log_message("Failed to apply second binary patch");
                return EXIT_FAILURE;
            }
        }

        /* 5a. Edit /mnt/us/extensions/kindle_browser_patch/patched_bin/browser to replace the string in question */
        {
            const char *sed_cmd =
                "sed -i 's|exec chroot /chroot /usr/bin/chromium/bin/kindle_browser|"
                "/mnt/us/extensions/kindle_browser_patch/patched_bin/chromium/bin/kindle_browser|g' /mnt/us/extensions/kindle_browser_patch/patched_bin/browser";
            if (run_command(sed_cmd) != 0) {
                log_message("Failed to update browser binary location in /mnt/us/extensions/kindle_browser_patch/patched_bin/browser");
                return EXIT_FAILURE;
            }
        }
		
		/* 5b. Edit /mnt/us/extensions/kindle_browser_patch/patched_bin/browser to replace the string in question. This one applies to the Scribe on 5.17.3 */
        {
            const char *sed_cmd =
                "sed -i 's|exec /usr/bin/chromium/bin/kindle_browser|"
                "/mnt/us/extensions/kindle_browser_patch/patched_bin/chromium/bin/kindle_browser|g' /mnt/us/extensions/kindle_browser_patch/patched_bin/browser";
            if (run_command(sed_cmd) != 0) {
                log_message("Failed to update browser binary location in /mnt/us/extensions/kindle_browser_patch/patched_bin/browser");
                return EXIT_FAILURE;
            }
        }
		
        /* 6. Check the sqlite3 database value */
        {
            char db_value[256] = {0};
            const char *sql_query =
                "sqlite3 /var/local/appreg.db \"SELECT value FROM properties "
                "WHERE handlerId='com.lab126.browser' AND name='command';\"";
            if (run_command_capture(sql_query, db_value, sizeof(db_value)) != 0) {
                log_message("Failed to query sqlite3 database");
                return EXIT_FAILURE;
            }
            if (strcmp(db_value, "/usr/bin/browser -j") != 0) {
                log_message("Database check failed: expected '/usr/bin/browser -j', got '%s'", db_value);
                return EXIT_FAILURE;
            }
        }

        /* 7. Update the sqlite3 database value */
        {
            const char *sql_update =
                "sqlite3 /var/local/appreg.db \"UPDATE properties SET value='/mnt/us/extensions/kindle_browser_patch/patched_bin/browser -j' "
                "WHERE handlerId='com.lab126.browser' AND name='command';\"";
            if (run_command(sql_update) != 0) {
                log_message("Failed to update sqlite3 database");
                return EXIT_FAILURE;
            }
        }

        /* 8. Create an empty file at /mnt/us/extensions/kindle_browser_patch/installed */
        {
            FILE *f_inst = fopen("/mnt/us/extensions/kindle_browser_patch/installed", "w");
            if (!f_inst) {
                log_message("Failed to create /mnt/us/extensions/kindle_browser_patch/installed: %s", strerror(errno));
                return EXIT_FAILURE;
            }
            fclose(f_inst);
            log_message("Created empty file /mnt/us/extensions/kindle_browser_patch/installed");
        }

        /* 9. Run the lipc-set-prop command */
        if (run_command("lipc-set-prop com.lab126.appmgrd start app://com.lab126.browser") != 0) {
            log_message("Failed to run lipc-set-prop command");
            return EXIT_FAILURE;
        }

        log_message("Installation completed successfully");
    }
    else if (strcmp(argv[1], "uninstall") == 0) {
        log_message("Starting uninstallation process");

		/* Check if the installed file exists */
		int installed_file_exists = (access("/mnt/us/extensions/kindle_browser_patch/installed", F_OK) == 0);
		
		/* Check the sqlite3 database value if the installed file is not present */
		int db_value_valid = 0;
		if (!installed_file_exists) {
			char db_value[256] = {0};
			const char *sql_query =
				"sqlite3 /var/local/appreg.db \"SELECT value FROM properties "
				"WHERE handlerId='com.lab126.browser' AND name='command';\"";
			if (run_command_capture(sql_query, db_value, sizeof(db_value)) == 0) {
				if (strcmp(db_value, "/mnt/us/extensions/kindle_browser_patch/patched_bin/browser -j") == 0) {
					db_value_valid = 1;
				}
			}
		}

		/* Proceed only if either the file exists or the DB value is valid */
		if (!installed_file_exists && !db_value_valid) {
			log_message("Uninstall failed: it does not appear that the patch was installed in the first place");
			return EXIT_FAILURE;
		}

        /* 2. Reset the sqlite3 database value */
        {
            const char *sql_update =
                "sqlite3 /var/local/appreg.db \"UPDATE properties SET value='/usr/bin/browser -j' "
                "WHERE handlerId='com.lab126.browser' AND name='command';\"";
            if (run_command(sql_update) != 0) {
                log_message("Failed to update sqlite3 database during uninstallation");
                return EXIT_FAILURE;
            }
        }

        /* 3. Delete the file /mnt/us/extensions/kindle_browser_patch/installed */
        if (remove("/mnt/us/extensions/kindle_browser_patch/installed") != 0) {
            log_message("Failed to delete /mnt/us/extensions/kindle_browser_patch/installed: %s", strerror(errno));
            return EXIT_FAILURE;
        }
        log_message("Deleted file /mnt/us/extensions/kindle_browser_patch/installed");
		
	   /* 4. Delete the folder /mnt/us/extensions/kindle_browser_patch/patched_bin */
        if (run_command("rm -rf /mnt/us/extensions/kindle_browser_patch/patched_bin") != 0) {
            log_message("Failed to remove /mnt/us/extensions/kindle_browser_patch/patched_bin");
            return EXIT_FAILURE;
        }
        log_message("Deleted folder /mnt/us/extensions/kindle_browser_patch/patched_bin");
        log_message("Uninstallation completed successfully");
    }
    else {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
