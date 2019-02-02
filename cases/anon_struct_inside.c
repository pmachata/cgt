struct foo {
	union {
		struct {
			void (*cb)(struct foo *);
		};
	};
};

void fn(struct foo *foo) {
    foo->cb(foo);
}
