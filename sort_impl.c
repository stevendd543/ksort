#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/workqueue.h>

#include "list.h"
#include "sort.h"

static inline char *med3(char *, char *, char *, cmp_t *, void *);
static inline void swapfunc(char *, char *, int, int);
/* Timsort */
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __bulitin_expect(!!(x), 0)
#endif

#define MAX_MERGE_PENDING 85

// struct run {
//      list_head *list;
//      size_t len;
// }

// static struct list_head *merge(void *priv, list_cmp_func_t cmp,
// 				struct list_head *a, struct list_head *b)
// {
// 	struct list_head *head, **tail = &head;

// 	for (;;) {
// 		/* if equal, take 'a' -- important for sort stability */
// 		if (cmp(priv, a, b) <= 0) {
// 			*tail = a;
// 			tail = &a->next;
// 			a = a->next;
// 			if (!a) {
// 				*tail = b;
// 				break;
// 			}
// 		} else {
// 			*tail = b;
// 			tail = &b->next;
// 			b = b->next;
// 			if (!b) {
// 				*tail = a;
// 				break;
// 			}
// 		}
// 	}
// 	return head;
// }

// static void build_prev_link(struct list_head *head, struct list_head *tail,
// 			    struct list_head *list)
// {
// 	tail->next = list;
// 	do {
// 		list->prev = tail;
// 		tail = list;
// 		list = list->next;
// 	} while (list);

// 	/* The final links to make a circular doubly-linked list */
// 	tail->next = head;
// 	head->prev = tail;
// }

// static void merge_final(void *priv, list_cmp_func_t cmp, struct list_head
// *head, 			struct list_head *a, struct list_head *b)
// {
// 	struct list_head *tail = head;
// 	uint8_t count = 0;

// 	for (;;) {
// 		/* if equal, take 'a' -- important for sort stability */
// 		if (cmp(priv, a, b) <= 0) {
// 			tail->next = a;
// 			a->prev = tail;
// 			tail = a;
// 			a = a->next;
// 			if (!a)
// 				break;
// 		} else {
// 			tail->next = b;
// 			b->prev = tail;
// 			tail = b;
// 			b = b->next;
// 			if (!b) {
// 				b = a;
// 				break;
// 			}
// 		}
// 	}

// 	/* Finish linking remainder of list b on to tail */
// 	build_prev_link(head, tail, b);
// }

// static struct list_head *find_run(void *priv, struct list_head *list,
// 				  size_t *len, list_cmp_func_t cmp)
// {
// 	*len = 1;
// 	struct list_head *next = list->next;

// 	if (unlikely(next == NULL))
// 		return NULL;

// 	if (cmp(priv, list, next) > 0) {
// 		/* decending run, also reverse the list */
// 		struct list_head *prev = NULL;
// 		do {
// 			(*len)++;
// 			list->next = prev;
// 			prev = list;
// 			list = next;
// 			next = list->next;
// 		} while (next && cmp(priv, list, next) > 0);
// 		list->next = prev;
// 	} else {
// 		do {
// 			(*len)++;
// 			list = next;
// 			next = list->next;
// 		} while (next && cmp(priv, list, next) <= 0);
// 		list->next = NULL;
// 	}

// 	return next;
// }

// static void merge_at(void *priv, list_cmp_func_t cmp, struct run *at)
// {
// 	at[0].list = merge(priv, cmp, at[0].list, at[1].list);
// 	at[0].len += at[1].len;
// }

// static struct run *merge_force_collapse(void *priv, list_cmp_func_t cmp,
// 					struct run *stk, struct run *tp)
// {
// 	while ((tp - stk + 1) >= 3) {
// 		if (tp[-2].len < tp[0].len) {
// 			merge_at(priv, cmp, &tp[-2]);
// 			tp[-1] = tp[0];
// 		} else {
// 			merge_at(priv, cmp, &tp[-1]);
// 		}
// 		tp--;
// 	}
// 	return tp;
// }

// static struct run *merge_collapse(void *priv, list_cmp_func_t cmp,
// 				  struct run *stk, struct run *tp)
// {
// 	int n;
// 	while ((n = tp - stk + 1) >= 2) {
// 		if ((n >= 3 && tp[-2].len <= tp[-1].len + tp[0].len) ||
// 		    (n >= 4 && tp[-3].len <= tp[-2].len + tp[-1].len)) {
// 			if (tp[-2].len < tp[0].len) {
// 				merge_at(priv, cmp, &tp[-2]);
// 				tp[-1] = tp[0];
// 			} else {
// 				merge_at(priv, cmp, &tp[-1]);
// 			}
// 		} else if (tp[-1].len <= tp[0].len) {
// 			merge_at(priv, cmp, &tp[-1]);
// 		} else {
// 			break;
// 		}
// 		tp--;
// 	}

// 	return tp;
// }


/* Qsort routine from Bentley & McIlroy's "Engineering a Sort Function" */
#define swapcode(TYPE, parmi, parmj, n) \
    {                                   \
        long i = (n) / sizeof(TYPE);    \
        TYPE *pi = (TYPE *) (parmi);    \
        TYPE *pj = (TYPE *) (parmj);    \
        do {                            \
            TYPE t = *pi;               \
            *pi++ = *pj;                \
            *pj++ = t;                  \
        } while (--i > 0);              \
    }

static inline void swapfunc(char *a, char *b, int n, int swaptype)
{
    if (swaptype <= 1)
        swapcode(long, a, b, n) else swapcode(char, a, b, n)
}

#define q_swap(a, b)                       \
    do {                                   \
        if (swaptype == 0) {               \
            long t = *(long *) (a);        \
            *(long *) (a) = *(long *) (b); \
            *(long *) (b) = t;             \
        } else                             \
            swapfunc(a, b, es, swaptype);  \
    } while (0)

#define vecswap(a, b, n)                 \
    do {                                 \
        if ((n) > 0)                     \
            swapfunc(a, b, n, swaptype); \
    } while (0)

#define CMP(t, x, y) (cmp((x), (y)))

static inline char *med3(char *a,
                         char *b,
                         char *c,
                         cmp_t *cmp,
                         __attribute__((unused)) void *thunk)
{
    return CMP(thunk, a, b) < 0
               ? (CMP(thunk, b, c) < 0 ? b : (CMP(thunk, a, c) < 0 ? c : a))
               : (CMP(thunk, b, c) > 0 ? b : (CMP(thunk, a, c) < 0 ? a : c));
}
struct common {
    int swaptype; /* Code to use for swapping */
    size_t es;    /* Element size. */
    cmp_t *cmp;   /* Comparison function */
};

struct qsort {
    struct work_struct w;
    struct common *common;
    void *a;
    size_t n;
};
#define thunk NULL
static void qsort_algo(struct work_struct *w);
// static void init_timsort(void *elems, size_t size) {}
static void init_qsort(struct qsort *q,
                       void *elems,
                       size_t size,
                       struct common *common)
{
    INIT_WORK(&q->w, qsort_algo);
    q->a = elems;
    q->n = size;
    q->common = common;
}

static void qsort_algo(struct work_struct *w)
{
    struct qsort *qs = container_of(w, struct qsort, w);

    bool do_free = true;
    char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
    int d, r, swaptype, swap_cnt;
    void *a;      /* Array of elements. */
    size_t n, es; /* Number of elements; size. */
    cmp_t *cmp;
    size_t nl, nr;
    struct common *c;

    /* Initialize qsort arguments. */
    c = qs->common;
    es = c->es;
    cmp = c->cmp;
    swaptype = c->swaptype;
    a = qs->a;
    n = qs->n;
top:
    /* From here on qsort(3) business as usual. */
    swap_cnt = 0;
    if (n < 7) {
        for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
            for (pl = pm; pl > (char *) a && CMP(thunk, pl - es, pl) > 0;
                 pl -= es)
                q_swap(pl, pl - es);
        return;
    }
    pm = (char *) a + (n / 2) * es;
    if (n > 7) {
        pl = (char *) a;
        pn = (char *) a + (n - 1) * es;
        if (n > 40) {
            d = (n / 8) * es;
            pl = med3(pl, pl + d, pl + 2 * d, cmp, thunk);
            pm = med3(pm - d, pm, pm + d, cmp, thunk);
            pn = med3(pn - 2 * d, pn - d, pn, cmp, thunk);
        }
        pm = med3(pl, pm, pn, cmp, thunk);
    }
    q_swap(a, pm);
    pa = pb = (char *) a + es;

    pc = pd = (char *) a + (n - 1) * es;
    for (;;) {
        while (pb <= pc && (r = CMP(thunk, pb, a)) <= 0) {
            if (r == 0) {
                swap_cnt = 1;
                q_swap(pa, pb);
                pa += es;
            }
            pb += es;
        }
        while (pb <= pc && (r = CMP(thunk, pc, a)) >= 0) {
            if (r == 0) {
                swap_cnt = 1;
                q_swap(pc, pd);
                pd -= es;
            }
            pc -= es;
        }
        if (pb > pc)
            break;
        q_swap(pb, pc);
        swap_cnt = 1;
        pb += es;
        pc -= es;
    }

    pn = (char *) a + n * es;
    r = min(pa - (char *) a, pb - pa);
    vecswap(a, pb - r, r);
    r = min(pd - pc, pn - pd - (long) es);
    vecswap(pb, pn - r, r);

    if (swap_cnt == 0) { /* Switch to insertion sort */
        r = 1 + n / 4;   /* n >= 7, so r >= 2 */
        for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
            for (pl = pm; pl > (char *) a && CMP(thunk, pl - es, pl) > 0;
                 pl -= es) {
                q_swap(pl, pl - es);
                if (++swap_cnt > r)
                    goto nevermind;
            }
        return;
    }

nevermind:
    nl = (pb - pa) / es;
    nr = (pd - pc) / es;

    if (nl > 100 && nr > 100) {
        struct qsort *q = kmalloc(sizeof(struct qsort), GFP_KERNEL);
        init_qsort(q, a, nl, c);
        queue_work(workqueue, &q->w);
    } else if (nl > 0) {
        qs->a = a;
        qs->n = nl;
        /* The struct qsort is used for recursive call, so don't free it in
         * this iteration.
         */
        do_free = false;
        qsort_algo(w);
    }

    if (nr > 0) {
        a = pn - nr * es;
        n = nr;
        goto top;
    }

    if (do_free)
        kfree(qs);
}
/* timsort algorithm */
// void timsort(void *priv, struct list_head *head, list_cmp_func_t cmp)
// {
// 	struct list_head *list = head->next;
// 	struct run stk[MAX_MERGE_PENDING], *tp = stk - 1;

// 	if (head == head->prev)
// 		return;

// 	/* Convert to a null-terminated singly-linked list. */
// 	head->prev->next = NULL;

// 	do {
// 		tp++;
// 		/* Find next run */
// 		tp->list = list;
// 		list = find_run(priv, list, &tp->len, cmp);
// 		tp = merge_collapse(priv, cmp, stk, tp);
// 	} while (list);

// 	/* End of input; merge together all the runs. */
// 	tp = merge_force_collapse(priv, cmp, stk, tp);

// 	/* The final merge; rebuild prev links */
// 	if (tp > stk) {
// 		merge_final(priv, cmp, head, stk[0].list, stk[1].list);
// 	} else {
// 		build_prev_link(head, head, stk->list);
// 	}
// }
void sort_main(void *sort_buffer,
               size_t size,
               size_t es,
               cmp_t cmp,
               int sort_type)
{
    /* The allocation must be dynamic so that the pointer can be reliably freed
     * within the work function.
     */
    struct qsort *q = kmalloc(sizeof(struct qsort), GFP_KERNEL);
    struct common common = {
        .swaptype = ((char *) sort_buffer - (char *) 0) % sizeof(long) ||
                            es % sizeof(long)
                        ? 2
                    : es == sizeof(long) ? 0
                                         : 1,
        .es = es,
        .cmp = cmp,
    };

    init_qsort(q, sort_buffer, size, &common);

    queue_work(workqueue, &q->w);

    /* Ensure completion of all work before proceeding, as reliance on objects
     * allocated on the stack necessitates this. If not, there is a risk of
     * the work item referencing a pointer that has ceased to exist.
     */
    drain_workqueue(workqueue);
}
