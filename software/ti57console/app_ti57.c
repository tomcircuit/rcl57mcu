#include <stdio.h>
#include <string.h>

#include "ti57.h"
#include "utils57.h"
#include "mux57.h"

static char *get_aos(ti57_t *ti57, char *str)
{
    char *stack;
    int k = 0;

    stack = ti57_get_aos_stack(ti57);
    for (int i = 0; ; i++) {
        ti57_reg_t *reg = 0;
        char part[25];
        char c = stack[i];

        if (c == 0) break;

        switch(c) {
        case '0':
        case '1':
        case '2':
        case '3':
            reg = &ti57->X[c - '0'];
            break;
        case 'X':
            reg = ti57_get_regX(ti57);
            break;
        }
        if (reg) {
            char *user_reg_str = utils57_user_reg_to_str(reg,
                                                      ti57_is_sci(ti57),
                                                      ti57_get_fix(ti57));
            strcpy(part, user_reg_str);
        } else if (c == 'd') {
            strcpy(part, utils57_trim(ti57_get_display(ti57)));
        } else {
            int j = 0;
            if (c != '(') part[j++] = ' ';
            part[j++] = c;
            if (c != '(') part[j++] = ' ';
            part[j] = 0;
        }
        int len = strlen(part);
        memcpy(str + k, part, len);
        k += len;
    }
    str[k] = 0;
    return str;
}

static char *get_op_str(ti57_t *ti57, int step, char *str)
{
    op57_t *instruction = ti57_get_program_op(ti57, step);

    sprintf(str, "%s %s %c",
            instruction->inv ? "-" : " ",
            key57_get_ascii_name(instruction->key),
            (instruction->d >= 0) ? '0' + instruction->d : ' ');
    return str;
}

static void print_state(ti57_t *ti57)
{
    static char *MODES[] = {"EVAL", "LRN", "RUN"};
    static char *TRIGS[] = {"DEG", "RAD", "GRAD"};
    char str[1000];

    printf("INTERNAL STATE\n");
    printf("  A  = %s\n", utils57_reg_to_str(ti57->A));
    printf("  B  = %s\n", utils57_reg_to_str(ti57->B));
    printf("  C  = %s\n", utils57_reg_to_str(ti57->C));
    printf("  D  = %s\n", utils57_reg_to_str(ti57->D));
    printf("\n");

    for (int i = 0; i < 8; i++)
        printf("  X%d = %s\n", i, utils57_reg_to_str(ti57->X[i]));
    printf("\n");
    for (int i = 0; i < 8; i++)
        printf("  Y%d = %s\n", i, utils57_reg_to_str(ti57->Y[i]));
    printf("\n");

    printf("  R5=x%02x   RAB=%d\n", ti57->R5, ti57->RAB);
    printf("  COND=%d   hex=%d\n", ti57->COND, ti57->is_hex);
    printf("  pc=x%03x  stack=[x%03x, x%03x, x%03x]\n",
           ti57->pc, ti57->stack[0], ti57->stack[1], ti57->stack[2]);

    printf("\nMODES\n  %s %s Fix=%d\n",
           MODES[ti57_get_mode(ti57)],
           TRIGS[ti57_get_trig(ti57)],
           ti57_get_fix(ti57));

    printf("\nFLAGS\n  2nd[%s] Inv[%s] Sci[%s] Err[%s] NumEdit[%s]\n",
           ti57_is_2nd(ti57) ? "x" : " ",
           ti57_is_inv(ti57) ? "x" : " ",
           ti57_is_sci(ti57) ? "x" : " ",
           ti57_is_error(ti57) ? "x" : " ",
           ti57_is_number_edit(ti57) ? "x" : " ");

    printf("\nREGISTERS\n");
    printf("  X  = %s\n",
           utils57_user_reg_to_str(ti57_get_regX(ti57), false, 9));
    printf("  T  = %s\n",
           utils57_user_reg_to_str(ti57_get_regT(ti57), false, 9));
    for (int i = 0; i <= 7; i++) {
        printf("  R%d = %s\n", i,
               utils57_user_reg_to_str(ti57_get_user_reg(ti57, i), false, 9));
    }

    printf("\nAOS\n");
    printf("  aos_stack = %s\n", ti57_get_aos_stack(ti57));
    printf("  aos = %s\n", get_aos(ti57, str));

    int last_step = 49;
    while (last_step >= 0 && ti57_get_program_op(ti57, last_step)->key == 0)
        last_step -= 1;

    printf("\nPROGRAM (%d steps)\n", last_step + 1);
    for (int i = 0; i <= last_step; i++)
        printf("  %02d %s\n", i, get_op_str(ti57, i, str));
    printf("  pc=%d\n", ti57_get_program_pc(ti57));

    printf("\nDISP = [%s]\n", ti57_get_display(ti57));
}

static int digit_to_key_map[] = {82, 72, 73, 74, 62, 63, 64, 52, 53, 54};

static void run(ti57_t *ti57, int *keys, int n)
{
	unsigned char keycode = 0;

    // Init.
    utils57_burst_until_idle(ti57);
    printf("\r\nINIT COMPLETE");

    for (int i = 0; i < n; i++) {
        int key = keys[i] <= 9 ? digit_to_key_map[keys[i]] : keys[i];
        keycode = ((keys[i] / 10) << 4 ) | ((keys[i] % 10) & 0x0f);

        // Key Press.
        ti57_key_press(ti57, key / 10, key % 10);
		printf("\n\rKEYPRESS %s (# %x)",key57_get_ascii_name(keycode), (unsigned int)keycode);
        utils57_burst_until_idle(ti57);
        // Key Release.
        if (ti57->mode != TI57_LRN && key == 81) {
            // R/S
            utils57_burst_until_idle(ti57);  // Waiting for key release
            ti57_key_release(ti57);
            utils57_burst_until_busy(ti57);
            utils57_burst_until_idle(ti57);  // Waiting for key press after program run
			printf("\r\nRELEASE RUN");
        } else {
            ti57_key_release(ti57);
 			printf("\r\nRELEASE");  
			utils57_burst_until_idle(ti57);
        }
    }
}

int main(void)
{   
	unsigned int n = 0;
	printf("\n\rStarting TI-57 emulator\n");
	    int keys[] =
        // {21, 5, 24, 81, 21, 71, 81};  // program: sqrt(5)
        // {13, 13, 13, 13, 13, 13, 14};  // ln(ln(...(ln(0))...)).
        {1, 75, 2, 55, 3, 35, 4, 85};  // 1 + 2 * 3 ^ 4 = 
        // {5, 32, 5};  // 5 STO 5
    ti57_t ti57;

    ti57_init(&ti57);
 	in_register_dump = 0;

	printf("\n\rSTARTED");
	for(n = 0 ; n < 10000; ) {
		n += ti57_next(&ti57);
		printf("  CYC %d", n);
	}
	printf("\n\rFINISHED");

//    run(&ti57, keys, sizeof(keys)/sizeof(int));
//    print_state(&ti57);

	for (unsigned char s = 0; s < 8; s++) {
		unsigned short d = mux57_which_digits(&ti57.dA, &ti57.dB, s);
		printf("\r\nsegment %d : digits %04X", s, d);
		d = mux57_which_outputs(&ti57.dA, &ti57.dB, s);
		printf("\r\nsegment %d : outputs %04X", s, d);
	}
}
