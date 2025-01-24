# covent:: Async event loop for network programs

## Or: WHAT HAVE YOU DONE, DAVE?

It might be simplest to begin with a short bit of code:

```c++

covent::task<int> async_main() {
    auto hw = "Hello World!";
    for (auto c : hw) {
      std::cout << c;
      co_await covent::sleep(0.1);
    }
    std::cout << std::endl;
    co_return 0;
}

int main() {
    // Make a loop.
    covent::Loop loop;
    
    // Run the task.
    return loop.run_task(async_main());
}

```

That code will execute the coroutine in an event loop. The coroutine prints Hello World a character at a time, sleeping (not really - suspending) for a tenth of a second between each character.

You can do more fun than that - like open a TCP session to somewhere with `loop.add<Session>(...)`, and then calling session->connect(...) on the result. For which you'll need a sockaddr, which you can get from the resolver object - which you can configure to require DNSSEC, or inject weird records of your choosing into, or ...

And if you needed TLS, then of course there's a PKIX validator that's similarly tunable. And why yes, it will chase down DNSSEC records to add additional names, and fetch CRLs asynchronously, and ...

HTTP? Yes, there's an HTTP/1.1 library included, both server and client. The server is modelled as endpoints and middleware, so should be vaguely familiar to people who've seen Express.