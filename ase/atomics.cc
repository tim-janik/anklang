// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#include "atomics.hh"
#include "internal.hh"
#include "testing.hh"

#define ADEBUG(...)     Ase::debug ("atomics", __VA_ARGS__)

namespace {
using namespace Ase;

static const size_t N_THREADS = std::thread::hardware_concurrency();

TEST_INTEGRITY (atomic_bits_test);
static void
atomic_bits_test ()
{
  const size_t N = 37;
  AtomicBits a (37);
  TASSERT (a.all (0));
  TASSERT (!a.all (1));
  TASSERT (!a.any (1));
  TASSERT (a.any (0));
  for (size_t i = 0; i < N; i++) {
    TASSERT (a.any (0));
    TASSERT (a[i] == false);
    a.set (i, 1);
    TASSERT (a[i] == true);
    TASSERT (a.any (1));
    TASSERT (!a.all (0));
    a.set (i, 0);
    TASSERT (a[i] == false);
    TASSERT (!a.any (1));
    TASSERT (a.all (0));
  }
  // note, toggling all N bits is not sufficient to change bits in all blocks
  // i.e. toggle all a.size() bits for all() to flip
  for (size_t i = 0; i < a.size(); i++)
    a[i] ^= 1;
  TASSERT (a.all (1));
  TASSERT (!a.all (0));
  TASSERT (!a.any (0));
  TASSERT (a.any (1));
  for (size_t i = 0; i < N; i++) {
    TASSERT (a.all (1));
    TASSERT (a[i] == true);
    a[i] ^= 0;
    TASSERT (a.all (1));
    TASSERT (a[i] == true);
    a.set (i, 0);
    TASSERT (a[i] == false);
    TASSERT (a.any (0));
    TASSERT (!a.all (0));
    a[i] ^= 1;
    TASSERT (a[i] == true);
    TASSERT (!a.any (0));
    TASSERT (a.all (1));
  }
}

// Basic Unit test for AtomicIntrusiveStack<>
struct AisNode { int value = 0; std::atomic<AisNode*> next = nullptr; };
static std::atomic<AisNode*>&
atomic_next_ptrref (AisNode *node)
{
  return node->next;
}

TEST_INTEGRITY (atomic_stack_test);
static void
atomic_stack_test()
{
  bool was_empty;
  AtomicIntrusiveStack<AisNode> stack;
  TASSERT (stack.empty());
  AisNode n1, n2, n3;
  n1.value = 1;
  n2.value = 2;
  n3.value = 3;
  was_empty = stack.push (&n1);
  TASSERT (was_empty);
  n2.next = &n3;
  was_empty = stack.push_chain (&n2, &n3);
  TASSERT (!was_empty);
  AisNode *node = stack.pop_all();
  int sum = 0;
  while (node)
    {
      sum += node->value;
      node = node->next;
    }
  TASSERT (sum == 6);
}

// == MpmcStack<> test ==
static const size_t COUNTING_THREADS = N_THREADS + 1;
static constexpr const size_t NUMBER_NODES_PER_THREAD = 9999;
struct NumberNode {
  size_t number;
  Atomic<NumberNode*> intr_ptr_ = nullptr; // atomic intrusive pointer
};
using ConcurrentNumberStack = MpmcStack<NumberNode>;
static ConcurrentNumberStack number_stack;
static NumberNode *allocated_number_nodes = nullptr;
static Atomic<unsigned long long> number_totals = 0;
static void
run_count_number_nodes (NumberNode *nodes, ssize_t count)
{
  const pid_t tid = gettid();
  unsigned long long totals = 0, l = 0;
  ssize_t i = 0, r = count / 50, j = -r;
  while (i < count || j < count)
    {
      for (size_t t = 0; t < 77 && i < count; t++)
        {
          NumberNode *node = nodes + i;
          node->number = ++i;
          number_stack.push (node);
        }
      for (size_t t = 0; t < 37 && j < count; t++)
        {
          NumberNode *node = number_stack.pop();
          if (node)
            {
              totals += node->number;
              j++;
              if (r > 0 && (node->number & 0x1))
                {
                  r--;
                  node->number = 0;
                  number_stack.push (node); // ABA mixing
                }
            }
        }
      if (totals > l * 10)
        {
          l = totals;
          if (Test::verbose())
            printout ("thread %u: %llu\n", tid, totals);
        }
    }
  number_totals += totals;
}

TEST_INTEGRITY (mpmc_stack_test);
static void
mpmc_stack_test()
{
  allocated_number_nodes = (NumberNode*) calloc (COUNTING_THREADS * NUMBER_NODES_PER_THREAD, sizeof (NumberNode));
  assert (allocated_number_nodes);

  std::thread threads[COUNTING_THREADS];
  for (size_t i = 0; i < COUNTING_THREADS; i++)
    threads[i] = std::thread (run_count_number_nodes, allocated_number_nodes + i * NUMBER_NODES_PER_THREAD, NUMBER_NODES_PER_THREAD);
  for (size_t i = 0; i < COUNTING_THREADS; i++)
    threads[i].join();
  free (allocated_number_nodes);
  allocated_number_nodes = nullptr;
  assert (number_totals == COUNTING_THREADS * (NUMBER_NODES_PER_THREAD * (NUMBER_NODES_PER_THREAD + 1ull)) / 2);
}

} // Anon
