typedef void (*fp_t)(void);

static fp_t fn(void) {}

int main(int argc, char **argv)
{
    fn()();
}
