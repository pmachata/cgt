struct foo {
	union {
		struct {
			void (*cb)(void);
		};
	};
};

void fn(struct foo *foo) {
    foo->cb();
}
