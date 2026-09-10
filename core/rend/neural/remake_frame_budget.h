// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
namespace flycast::rend::neural {
// Render-thread budget for the remake lane (D-216). The600-frame gate requires
// that a slow lane fall back explicitly rather than slow emulation. With a
// budget (milliseconds of render-thread remake work per emulated frame) each
// frame adds that much credit; the scene feed runs whenever the credit is not
// negative and both the feed and the evaluation it leads to are charged their
// measured cost, so the lane runs every few frames (duty cycle budget over
// cost) and the frames in between are explicit feed skips. The evaluation of
// an image already returned is never deferred: a returned image is the
// consumer's finished work, and throttling the feed already bounds how many
// arrive. Credit is clamped to plus or minus two frames' worth so
// neither an idle stretch nor a one-time stall (a first-submit warmup of a
// second) can starve or flood the lane. A zero budget keeps the unlimited
// behavior. The policy changes when work runs, never what is accepted.
class RemakeFrameBudget {
 double budgetMs=0,credit=0,feedEma=0,evaluateEma=0;
 bool haveFeed=false,haveEvaluate=false;
 std::uint64_t feedRuns=0,feedSkips=0,evaluateRuns=0,evaluateDeferrals=0;
 static double learn(double ema,bool have,double sample){return have?ema*.8+sample*.2:sample;}
 void charge(double ms){if(Enabled())credit=(std::max)(credit-ms,-2*budgetMs);}
public:
 void Configure(double ms){budgetMs=ms>0?ms:0;credit=0;}
 bool Enabled()const{return budgetMs>0;}
 double BudgetMs()const{return budgetMs;}
 double Credit()const{return credit;}
 double FeedEstimateMs()const{return feedEma;}
 double EvaluateEstimateMs()const{return evaluateEma;}
 void BeginFrame(){if(Enabled())credit=(std::min)(credit+budgetMs,2*budgetMs);}
 bool AllowFeed()const{return !Enabled()||credit>=0;}
 bool AllowEvaluate()const{return true;} // Throttled through the feed, never by dropping finished work.
 void RecordFeed(double ms){feedEma=learn(feedEma,haveFeed,ms);haveFeed=true;++feedRuns;charge(ms);}
 void RecordEvaluate(double ms){evaluateEma=learn(evaluateEma,haveEvaluate,ms);haveEvaluate=true;++evaluateRuns;charge(ms);}
 void CountFeedSkip(){++feedSkips;}
 void CountEvaluateDeferral(){++evaluateDeferrals;}
 std::uint64_t FeedRuns()const{return feedRuns;}
 std::uint64_t FeedSkips()const{return feedSkips;}
 std::uint64_t EvaluateRuns()const{return evaluateRuns;}
 std::uint64_t EvaluateDeferrals()const{return evaluateDeferrals;}
};
}
