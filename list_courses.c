#include "common.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open(COURSE_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    Course course;
    int idx = 0;
    printf("Raw contents of %s:\n", COURSE_FILE);
    while (read(fd, &course, sizeof(Course)) == sizeof(Course)) {
        printf("[%d] ID: %d, Name: '%s', Faculty ID: %d, Seats: %d, Enrolled: %d\n",
               idx++, course.id, course.name, course.faculty_id, course.seats, course.enrolled);
    }
    close(fd);
    return 0;
} 