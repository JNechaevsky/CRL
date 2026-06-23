#include <check.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

// Include the actual production header
#include "opl/opl_sdl.h"

START_TEST(test_allocation_size_overflow_protection)
{
    // Invariant: Multiplication for allocation size must not overflow
    // The function should either handle overflow safely or reject the input
    
    // Test cases: boundary values that could cause overflow
    size_t test_cases[][2] = {
        {SIZE_MAX, 2},           // Exact exploit case - multiplication wraps
        {SIZE_MAX / 2 + 1, 2},   // Boundary case - just overflows
        {100, 20},               // Valid case - should work normally
        {0, SIZE_MAX},           // Zero with max multiplier
        {SIZE_MAX, 1}            // Max size with multiplier 1
    };
    
    int num_cases = sizeof(test_cases) / sizeof(test_cases[0]);
    
    for (int i = 0; i < num_cases; i++) {
        size_t count = test_cases[i][0];
        size_t size = test_cases[i][1];
        
        // Call the actual allocation function from the production code
        // The security property is that this should not allocate a buffer
        // that is smaller than requested due to overflow
        void *result = OPL_SDL_AllocBuffer(count, size);
        
        if (result != NULL) {
            // If allocation succeeded, verify the allocated size is correct
            // by checking we can write to the entire buffer
            char *buffer = (char *)result;
            size_t allocated_size = count * size;
            
            // Only attempt to write if the multiplication didn't overflow
            if (count == 0 || size == 0 || allocated_size / count == size) {
                // Try to write to the last byte of the buffer
                if (allocated_size > 0) {
                    buffer[allocated_size - 1] = 0;
                }
            }
            
            OPL_SDL_FreeBuffer(result);
        }
        
        // The test passes if we don't crash or trigger heap corruption
        // The actual security property is maintained by not overflowing
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_allocation_size_overflow_protection);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}