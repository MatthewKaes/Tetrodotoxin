# Async as an execution policy

This is a design for review, not an implemented API. Ordinary Data access,
Copy and Swizzle return their final results synchronously. An Async policy can
lift those operations into deferred execution without adding callbacks or a
pending request to their ordinary contracts.

The distinction is observable. Synchronous Copy stops using its target before
returning. Async submission returns an accepted task whose operation may not
have run yet. The Async owner must keep the operation, its inputs, its result
storage and the code implementing it alive through execution and delivery.

## What the layer wraps

The initial unit is a bound invocation: the work to perform plus a thunk that
knows its actual signature. The generic Async machinery calls that thunk. It
does not understand Copy, primitive types, argument lists or result formats.

A native adapter can lift `[] { return Flows::Copy::flow(flow, target); }`. A C adapter
can supply a function that invokes a foreign operation and writes its result
into the adapter's own state. The thunk alone interprets that state. Scheduling
does not grant anyone else permission to cast it.

This wraps arbitrary *supplied invocations*. It does not automatically convert
an unknown bound operation table into an asynchronous table. That would require
an authored or generated adapter for each signature. If a dialect lifts an
entire command surface, those adapters belong to that dialect or its compiler.
Returning early is a different completion contract, so an asynchronous surface
cannot advertise the ordinary synchronous identity and change its guarantees.

## The proposed shape

These names illustrate the division of responsibility. Exact declarations and
contract IDs need review before implementation.

```cpp
// The task owner supplies stable storage for the invocation and its result.
auto work = [&flow, target] { return Flows::Copy::flow(flow, target); };
Async::Task task(work);
Async::Task next(consume_copy_result);

// An asynchronous consumer composes its continuation through the same policy.
// A synchronous Copy consumer does not need this task or continuation.
task.then(next, delivery_executor);

// The executor is already a negotiated Semantic capability. Submission does
// not execute work on the caller's stack or wait for its result. The owner
// handles rejection before releasing or retrying these still idle tasks.
auto admission = task.submit(executor);
```

The corresponding C execution boundary can remain a state/thunk pair:

```c
struct invocation {
  void *source;
  void (*run)(void *source);
};

// run invokes the already bound operation and stores its typed result in
// state owned by that invocation. The executor never interprets that state.
submission submit(executor, task, invocation);
```

Semantic owns the Async agreement and its bindable executor contract. The C++
Task supplies a typed facade over its own invocation and result storage. The
executor implements queueing, execution affinity and notification delivery.
Data acquires no scheduler, task state, locks or Async dependency.

An implementation would live under `ttx/semantic/async/`: an executor contract
for admission and delivery, and a task owner for invocation/result lifetime.
Those are separate owners. Neither becomes a base class for Copy, and no Async
header is required by an ordinary synchronous operation.

Task is a caller-owned object kept at a stable address. A successful submission
borrows it until execution and delivery finish. Capturing a Storage or Query by
value copies a descriptor, not ownership of its bytes or module. Those owners
must be retained by the enclosing task scope or an explicit lifetime policy.
The first implementation should neither hide allocation nor block in a Task
destructor to repair an expired borrow.

## Submission and completion

The state progression is `Idle -> Queued -> Running -> Completed`. Rejected
submission leaves the task idle, executes nothing and retains no invocation.
Accepted work runs exactly once on the chosen executor. The task publishes its
result only after the synchronous invocation returns, then schedules its
registered continuation on the selected delivery executor.

Submission returns acceptance, not an operation result. That is the intended
meaning of an immediate Pending answer. A worker may finish before the caller
inspects the task, so acceptance must not promise that the task is *still*
pending at that later observation. If a host requires execution only on a later
event-loop turn, its executor must provide that stronger ordering.

The inner result remains intact. For example, a Copy that reports `IoError`
produces a completed Async task containing that exact status. Queue rejection
is a different outcome: Copy never ran. Async must not turn either case into
success or add progress information that Copy does not promise.

The same distinction applies to `BindingPending`: an invocation can finish by
reporting that its inner binding question is unresolved. The Async task is still
completed with that answer. Retrying the binding belongs to the owning policy,
not an automatic loop inside generic Async execution.

The initial model supports one continuation per task. Registering before or
after completion schedules that continuation exactly once. Registration and
result publication must share one ordering rule so no wakeup is lost. A threaded
executor needs release/acquire publication or equivalent locking; a serialized
host loop can provide the ordering without cross-thread synchronization.
Neither invokes user work while holding an internal state lock.

`then` or `map` composes typed operations within the asynchronous policy, with
caller-owned storage for the resulting task. A continuation runs while the
predecessor's result remains alive. Carrying a borrowed value further requires
the appropriate owner lifetime, just as it does for synchronous bindings.

## Waiting belongs to the consuming policy

No busy-wait loop is required or supplied. An asynchronous consumer registers
its next action and lets its host continue making progress. A Godot adapter can
deliver that action through the owning node's permitted execution context.

A blocking consumer needs an explicitly negotiated wait capability from the
runtime that knows how completion is driven. It can use an operating-system
wait where appropriate or reject a wait that would block the only execution
context able to finish the task. The generic Async policy does not guess how to
pump an unknown host. A language can later adapt this same contract to `await`.

Direct synchronous consumers retain `Flows::Copy::flow(flow, target) -> Result` and
pay none of the queue, notification or task-lifetime costs. Entering Async is
the explicit choice to accept those execution semantics.

Cancellation is not part of the first contract. An accepted invocation must
finish, and the owner must keep its dependencies alive. A later cancellation
policy would need to distinguish work removed before execution from work whose
side effects already occurred; it cannot simply destroy a running task.

## Existing runtime and the first proof

Perimortem's `Core::Thread::Worker` is an execution primitive, not this executor
contract. Its job bytes are copied and its destructor joins. That does not
retain objects referenced by those bytes, and spawning one Worker per command
would impose unrelated startup costs. A persistent worker-backed executor can
be a runtime implementation; a deterministic owner-driven queue is sufficient
for the first composition test. Neither choice belongs inside Copy.

The acceptance examples should exercise the same Async policy with:

| Case | Observable requirement |
| --- | --- |
| Ordinary synchronous Copy | Ready result at return, no task or callback machinery |
| Async Copy | Submission returns before Copy runs; driving the executor produces the same result |
| Foreign C operation | A C invocation with a different result uses the same queueing and delivery contract |
| Two tasks with different targets | Each retains its own invocation and result; execution order does not swap destinations |
| Domain failure | The exact inner failure survives asynchronous delivery without adding a progress contract |
| Rejected submission | No invocation, result write or retained borrow |
| Continuation registered before/after completion | Exactly one delivery in both cases, without polling |
| Task/module lifetime | State and code stay alive through execution and result consumption |
| Execution affinity | Work and continuation run only in their negotiated contexts |
| Composition | A typed continuation consumes one result and produces another without modifying either underlying synchronous operation |

These examples would establish a reusable execution policy. They would not yet
prove an automatic proxy for every interface, a universal blocking wait, a
coroutine runtime or an arbitrary cancellation mechanism. Those remain separate
agreements rather than hidden obligations of ordinary data transfer.
