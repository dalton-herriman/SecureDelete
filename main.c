#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // For unlink, fsync, close, lseek
#include <fcntl.h>      // For open
#include <sys/stat.h>   // For fstat
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 4096  // 4 KB

/* Deletes data by overwriting it multiple times */
int secure_delete(const char *filename, int passes) {
    int fd = open(filename, O_WRONLY);  // Open in write-only
    if (fd < 0) {                       // Catch error during open
        perror("Error opening file");
        return 1;
    }

    /* Get file size using fstat */
    struct stat st;
    if (fstat(fd, &st) != 0) {     
        perror("Error getting file size");
        close(fd);
        return 1;
    }
    
    off_t filesize = st.st_size;  // Store the size of the file
    char buffer[BUFFER_SIZE];     // Buffer to hold random bytes

    /* Open /dev/urandom to read random bytes */
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (!urandom) {
        perror("Failed to open /dev/urandom");
        close(fd);
        return 1;
    }

    for (int pass = 0; pass < passes; pass++) {
        /* Seek back to the beginning of the file before overwriting */
        if (lseek(fd, 0, SEEK_SET) == -1) {
            perror("Error seeking to start");
            fclose(urandom);
            close(fd);
            return 1;
        }

        off_t bytes_written = 0;  // Track how many bytes we've written so far

        /* Write random data until entire file is overwritten */
        while (bytes_written < filesize) {
            size_t to_write = BUFFER_SIZE; // Calculate how many bytes to write this iteration
            if (filesize - bytes_written < BUFFER_SIZE)
                to_write = filesize - bytes_written;

            /* Read random bytes from /dev/urandom into buffer */
            if (fread(buffer, 1, to_write, urandom) != to_write) {
                perror("Error reading random data");
                fclose(urandom);
                close(fd);
                return 1;
            }

            /* Write the random bytes to the file */
            ssize_t written = write(fd, buffer, to_write);
            if (written < 0) {
                perror("Error writing to file");
                fclose(urandom);
                close(fd);
                return 1;
            }

            bytes_written += written;
        }

        /* Flush written data to disk to ensure it's not cached */
        if (fsync(fd) != 0) {
            perror("Error syncing data");
            fclose(urandom);
            close(fd);
            return 1;
        }
    }

    /* Close the random source and the file descriptor */
    fclose(urandom);
    close(fd);

    /* Finally, delete the file from the system */
    if (unlink(filename) != 0) {
        perror("Error deleting file");
        return 1;
    }

    printf("Successfully securely deleted '%s' with %d passes.\n", filename, passes);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { // Check if filename is provided
        fprintf(stderr, "Usage: %s <file> [passes]\n", argv[0]);
        return 1;
    }

    int passes = 3;  // Default number of overwrite passes
    /* If passes provided, parse it */
    if (argc >= 3) {
        passes = atoi(argv[2]);
        if (passes <= 0) {
            fprintf(stderr, "Invalid number of passes. Using default (3).\n");
            passes = 3;
        }
    }

    /* Call the secure delete function with filename and passes */
    return secure_delete(argv[1], passes);
}
