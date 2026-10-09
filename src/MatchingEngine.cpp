#include <MatchingEngine.h>


MatchingEngine::MatchingEngine()
    : orderBook_(),
        running_(false)
{}


void MatchingEngine::start()
{
    running_.store(true);

    matchingThread_ =
        std::thread(&MatchingEngine::run, this);
}