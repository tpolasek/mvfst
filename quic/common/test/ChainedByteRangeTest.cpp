/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <folly/String.h>
#include <folly/io/Cursor.h>
#include <quic/common/ChainedByteRange.h>

using namespace std;
using namespace folly;
using namespace quic;

#define SCL(x) (x), sizeof(x) - 1

namespace {
void checkConsistency(const ChainedByteRangeHead& queue) {
  size_t len = queue.head.computeChainDataLength();
  EXPECT_EQ(len, queue.chainLength());

  ChainedByteRange* current = queue.head.getNext();
  EXPECT_EQ(queue.head.getNext()->getPrev(), &queue.head);
  EXPECT_EQ(queue.head.getPrev()->getNext(), &queue.head);
  while (current != &queue.head) {
    EXPECT_EQ(current->getNext()->getPrev(), current);
    EXPECT_EQ(current->getPrev()->getNext(), current);
    current = current->getNext();
  }
}

} // namespace

static auto kHello = IOBuf::copyBuffer(SCL("Hello"));
static auto kCommaSpace = IOBuf::copyBuffer(SCL(", "));
static auto kComma = IOBuf::copyBuffer(SCL(","));
static auto kSpace = IOBuf::copyBuffer(SCL(" "));
static auto kEmpty = IOBuf::copyBuffer(SCL(""));
static auto kWorld = IOBuf::copyBuffer(SCL("World"));

TEST(ChainedByteRangeHead, AppendBasic) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  queue.append(kSpace);
  checkConsistency(queue);
  EXPECT_EQ(queue.head.computeChainDataLength(), 6);
}

TEST(ChainedByteRangeHead, Append) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  ChainedByteRangeHead queue2;
  queue2.append(kCommaSpace);
  queue2.append(kWorld);
  checkConsistency(queue);
  checkConsistency(queue2);
}

TEST(ChainedByteRangeHead, Append2) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  ChainedByteRangeHead queue2;
  queue2.append(kCommaSpace);
  queue2.append(kWorld);
  checkConsistency(queue);
  checkConsistency(queue2);
}

TEST(ChainedByteRangeHead, AppendHead) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  ChainedByteRangeHead queue2;
  queue2.append(kCommaSpace);
  queue.append(std::move(queue2));
  checkConsistency(queue);
  EXPECT_EQ(queue.head.computeChainDataLength(), 7);
}

TEST(ChainedByteRangeHead, AppendHead2) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  queue.append(kComma);
  ChainedByteRangeHead queue2;
  queue2.append(kSpace);
  queue2.append(kWorld);
  queue.append(std::move(queue2));
  checkConsistency(queue);
  EXPECT_EQ(queue.head.computeChainDataLength(), 12);
}

TEST(ChainedByteRangeHead, AppendHead3) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  queue.append(kComma);
  ChainedByteRangeHead queue2;
  queue2.append(kSpace);
  queue.append(std::move(queue2));
  checkConsistency(queue);
  EXPECT_EQ(queue.head.computeChainDataLength(), 7);
}

TEST(ChainedByteRangeHead, AppendHead4) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  ChainedByteRangeHead queue2;
  queue2.append(kComma);
  queue2.append(kSpace);
  queue.append(std::move(queue2));
  checkConsistency(queue);
  EXPECT_EQ(queue.head.computeChainDataLength(), 7);
}

TEST(ChainedByteRangeHead, AppendMultipleEmpty) {
  auto buf = folly::IOBuf::copyBuffer("");
  buf->appendToChain(folly::IOBuf::copyBuffer(""));
  buf->appendToChain(folly::IOBuf::copyBuffer("apple"));
  buf->appendToChain(folly::IOBuf::copyBuffer("ball"));
  buf->appendToChain(folly::IOBuf::copyBuffer(""));
  buf->appendToChain(folly::IOBuf::copyBuffer("dog"));
  buf->appendToChain(folly::IOBuf::copyBuffer("cat"));

  ChainedByteRangeHead chainedByteRangeHead;
  chainedByteRangeHead.append(buf);
  EXPECT_EQ(chainedByteRangeHead.chainLength(), 15);
  EXPECT_EQ(chainedByteRangeHead.head.toStr(), "appleballdogcat");
}

TEST(ChainedByteRangeHead, AppendStringPiece) {
  std::string s("Hello, World");
  auto helloWorld = IOBuf::copyBuffer(s);
  ChainedByteRangeHead queue;
  ChainedByteRangeHead queue2;
  queue.append(helloWorld);
  queue2.append(helloWorld);
  checkConsistency(queue);
  checkConsistency(queue2);
  EXPECT_EQ(s.length(), queue.head.computeChainDataLength());
  EXPECT_EQ(s.length(), queue2.head.computeChainDataLength());
  EXPECT_EQ(
      0,
      memcmp(
          queue.head.getRange().data(),
          queue2.head.getRange().data(),
          s.length()));
}

TEST(ChainedByteRangeHead, Splitttt) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  queue.append(kComma);
  queue.append(kSpace);
  queue.append(kEmpty);
  queue.append(kWorld);
  checkConsistency(queue);
  EXPECT_EQ(12, queue.head.computeChainDataLength());

  auto prefix = queue.splitAtMost(1);
  checkConsistency(queue);
  EXPECT_EQ(1, prefix.head.computeChainDataLength());
  EXPECT_EQ(11, queue.head.computeChainDataLength());
  EXPECT_EQ(prefix.head.toStr(), "H");
  ChainedByteRangeHead rch1(std::move(prefix));

  prefix = queue.splitAtMost(2);
  checkConsistency(queue);
  EXPECT_EQ(2, prefix.head.computeChainDataLength());
  EXPECT_EQ(9, queue.head.computeChainDataLength());
  EXPECT_EQ(prefix.head.toStr(), "el");
  ChainedByteRangeHead rch2(std::move(prefix));

  prefix = queue.splitAtMost(3);
  checkConsistency(queue);
  EXPECT_EQ(3, prefix.head.computeChainDataLength());
  EXPECT_EQ(6, queue.head.computeChainDataLength());
  EXPECT_EQ(prefix.head.toStr(), "lo,");
  ChainedByteRangeHead rch3(std::move(prefix));

  prefix = queue.splitAtMost(1);
  checkConsistency(queue);
  EXPECT_EQ(1, prefix.head.computeChainDataLength());
  EXPECT_EQ(5, queue.head.computeChainDataLength());
  EXPECT_EQ(prefix.head.toStr(), " ");
  ChainedByteRangeHead rch4(std::move(prefix));

  prefix = queue.splitAtMost(5);
  checkConsistency(queue);
  EXPECT_EQ(5, prefix.head.computeChainDataLength());
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.chainLength(), 0);
  EXPECT_TRUE(queue.head.getRange().empty());
  EXPECT_EQ(prefix.head.toStr(), "World");
  ChainedByteRangeHead rch5(std::move(prefix));

  auto helloComma = IOBuf::copyBuffer(SCL("Hello,"));
  queue.append(helloComma);
  checkConsistency(queue);
  prefix = queue.splitAtMost(3);
  checkConsistency(queue);
  EXPECT_EQ(3, prefix.head.computeChainDataLength());
  EXPECT_EQ(3, queue.chainLength());
  EXPECT_EQ(prefix.head.toStr(), "Hel");
  ChainedByteRangeHead rch6(std::move(prefix));

  auto spaceWorld = IOBuf::copyBuffer(SCL(" World"));
  queue.append(spaceWorld);
  checkConsistency(queue);
  prefix = queue.splitAtMost(13);
  EXPECT_EQ(9, prefix.head.computeChainDataLength());
  EXPECT_EQ(0, queue.chainLength());
  EXPECT_EQ(prefix.head.toStr(), "lo, World");
  checkConsistency(queue);
  ChainedByteRangeHead rch7(std::move(prefix));
}

TEST(ChainedByteRangeHead, Empty) {
  ChainedByteRangeHead emptyQueue;
  checkConsistency(emptyQueue);
  EXPECT_TRUE(emptyQueue.empty());
  EXPECT_EQ(emptyQueue.chainLength(), 0);

  emptyQueue.append(folly::IOBuf::copyBuffer("apple"));
  checkConsistency(emptyQueue);
  EXPECT_FALSE(emptyQueue.head.empty());
}

TEST(ChainedByteRangeHead, FromIobuf) {
  auto buf = folly::IOBuf::copyBuffer("");
  buf->appendToChain(folly::IOBuf::copyBuffer(""));
  buf->appendToChain(folly::IOBuf::copyBuffer("apple"));
  buf->appendToChain(folly::IOBuf::copyBuffer("ball"));
  buf->appendToChain(folly::IOBuf::copyBuffer(""));
  buf->appendToChain(folly::IOBuf::copyBuffer("dog"));
  buf->appendToChain(folly::IOBuf::copyBuffer("cat"));

  ChainedByteRangeHead chainedByteRangeHead(buf);
  EXPECT_EQ(chainedByteRangeHead.chainLength(), 15);
  EXPECT_EQ(chainedByteRangeHead.head.toStr(), "appleballdogcat");
}

TEST(ChainedByteRangeHead, FromIobufEmpty) {
  auto buf = folly::IOBuf::copyBuffer("");
  ChainedByteRangeHead chainedByteRangeHead(buf);
  EXPECT_TRUE(chainedByteRangeHead.empty());
}

TEST(ChainedByteRangeHead, TrimStart) {
  auto cbr = std::make_unique<ChainedByteRange>(
      folly::ByteRange(kHello->data(), kHello->length()));
  cbr->trimStart(3);
  EXPECT_EQ(cbr->toStr(), "lo");
}

TEST(ChainedByteRangeHead, SplitHeadFromChainOfOne) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  checkConsistency(queue);
  EXPECT_EQ(5, queue.head.computeChainDataLength());

  auto prefix = queue.splitAtMost(3);
  checkConsistency(queue);
  EXPECT_EQ(3, prefix.head.computeChainDataLength());
  EXPECT_EQ(2, queue.head.computeChainDataLength());
  EXPECT_EQ(prefix.head.toStr(), "Hel");
  EXPECT_EQ(queue.head.toStr(), "lo");
}

TEST(ChainedByteRangeHead, MoveAssignmentOperator) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  queue.append(kWorld);

  ChainedByteRangeHead queue2 = std::move(queue);
  EXPECT_EQ(queue2.chainLength(), 10);
  EXPECT_TRUE(queue.empty());
}

TEST(ChainedByteRangeHead, SplitHeadFromChainOfTwo) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  queue.append(kWorld);
  checkConsistency(queue);
  EXPECT_EQ(10, queue.head.computeChainDataLength());

  auto prefix = queue.splitAtMost(3);
  checkConsistency(queue);
  EXPECT_EQ(3, prefix.head.computeChainDataLength());
  EXPECT_EQ(7, queue.head.computeChainDataLength());
  EXPECT_EQ(prefix.head.toStr(), "Hel");
  EXPECT_EQ(queue.head.toStr(), "loWorld");
}

TEST(ChainedByteRangeHead, SplitOneAndHalfFromChainOfTwo) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  queue.append(kWorld);
  checkConsistency(queue);
  EXPECT_EQ(10, queue.head.computeChainDataLength());

  auto prefix = queue.splitAtMost(7);
  checkConsistency(queue);
  EXPECT_EQ(7, prefix.head.computeChainDataLength());
  EXPECT_EQ(3, queue.head.computeChainDataLength());
  EXPECT_EQ(prefix.head.toStr(), "HelloWo");
  EXPECT_EQ(queue.head.toStr(), "rld");
}

TEST(ChainedByteRangeHead, SplitOneAndHalfFromChainOfThree) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  queue.append(kWorld);
  queue.append(kHello);
  checkConsistency(queue);
  EXPECT_EQ(15, queue.head.computeChainDataLength());

  auto prefix = queue.splitAtMost(7);
  checkConsistency(queue);
  EXPECT_EQ(7, prefix.head.computeChainDataLength());
  EXPECT_EQ(8, queue.head.computeChainDataLength());
  EXPECT_EQ(prefix.head.toStr(), "HelloWo");
  EXPECT_EQ(queue.head.toStr(), "rldHello");
}

TEST(ChainedByteRangeHead, SplitOneAndHalfFromChainOfFour) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  queue.append(kWorld);
  queue.append(kHello);
  queue.append(kWorld);
  checkConsistency(queue);
  EXPECT_EQ(20, queue.head.computeChainDataLength());

  auto prefix = queue.splitAtMost(7);
  checkConsistency(queue);
  EXPECT_EQ(7, prefix.head.computeChainDataLength());
  EXPECT_EQ(13, queue.head.computeChainDataLength());
  EXPECT_EQ(prefix.head.toStr(), "HelloWo");
  EXPECT_EQ(queue.head.toStr(), "rldHelloWorld");
}

TEST(ChainedByteRangeHead, SplitZero) {
  ChainedByteRangeHead queue;
  auto helloWorld = IOBuf::copyBuffer(SCL("Hello world"));
  queue.append(helloWorld);
  auto splitRch = queue.splitAtMost(0);
  EXPECT_EQ(splitRch.head.computeChainDataLength(), 0);
}

TEST(ChainedByteRangeHead, SplitEmpty) {
  ChainedByteRangeHead queue;
  auto splitRch = queue.splitAtMost(0);
  EXPECT_EQ(splitRch.head.computeChainDataLength(), 0);
}

TEST(ChainedByteRangeHead, SplitEmptt) {
  ChainedByteRangeHead queue;
  auto splitRch = queue.splitAtMost(1);
  EXPECT_EQ(splitRch.head.computeChainDataLength(), 0);
}

TEST(ChainedByteRangeHead, TrimStartAtMost) {
  ChainedByteRangeHead queue;
  queue.append(kHello);
  auto prefixLen = queue.trimStartAtMost(3);
  EXPECT_EQ(3, prefixLen);
  EXPECT_EQ(2, queue.chainLength());
  checkConsistency(queue);

  prefixLen = queue.trimStartAtMost(2);
  EXPECT_EQ(2, prefixLen);
  EXPECT_EQ(0, queue.chainLength());
  checkConsistency(queue);

  queue.append(kHello);
  queue.append(kWorld);
  prefixLen = queue.trimStartAtMost(7);
  EXPECT_EQ(7, prefixLen);
  EXPECT_EQ(3, queue.chainLength());
  checkConsistency(queue);

  prefixLen = queue.trimStartAtMost(10);
  EXPECT_EQ(3, prefixLen);
  EXPECT_EQ(0, queue.chainLength());
  checkConsistency(queue);

  queue.append(kHello);
  queue.append(kWorld);

  prefixLen = queue.trimStartAtMost(12);
  EXPECT_EQ(10, prefixLen);
  EXPECT_EQ(0, queue.chainLength());
  checkConsistency(queue);

  queue.append(kHello);
  queue.append(kWorld);
  queue.append(kHello);

  prefixLen = queue.trimStartAtMost(12);
  EXPECT_EQ(12, prefixLen);
  EXPECT_EQ(3, queue.chainLength());
  checkConsistency(queue);
}

TEST(ChainedByteRangeHead, TrimStartOneByte) {
  ChainedByteRangeHead queue;
  auto h = IOBuf::copyBuffer(SCL("H"));
  queue.append(h);
  checkConsistency(queue);
  queue.trimStartAtMost(1);
  checkConsistency(queue);
}

TEST(ChainedByteRangeHead, TrimStartClearChain) {
  ChainedByteRangeHead queue;
  constexpr string_view alphabet = "abcdefghijklmnopqrstuvwxyz";
  auto buf = IOBuf::copyBuffer(alphabet);
  queue.append(buf);
  queue.append(buf);
  // validate chain length
  const size_t expectedChainLength = alphabet.size() * 2;
  EXPECT_EQ(queue.chainLength(), expectedChainLength);
  checkConsistency(queue);
  // attempt to trim more than chainLength
  queue.trimStartAtMost(expectedChainLength + 1);
  checkConsistency(queue);
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.chainLength(), 0);
  EXPECT_TRUE(queue.head.empty());
}

TEST(ChainedByteRangeHead, TestEmptyWithMiddleEmptyBuffer) {
  ChainedByteRangeHead queue1;
  queue1.append(kHello);
  ChainedByteRangeHead queue2;
  queue1.append(std::move(queue2));
  queue1.append(kWorld);
  EXPECT_FALSE(queue1.head.getNext()->empty());
}

TEST(ChainedByteRangeHead, TestMove) {
  auto buf = folly::IOBuf::copyBuffer("corporate america");
  buf->appendToChain(folly::IOBuf::copyBuffer("apple"));
  buf->appendToChain(folly::IOBuf::copyBuffer("ball"));
  buf->appendToChain(folly::IOBuf::copyBuffer("dog"));
  buf->appendToChain(folly::IOBuf::copyBuffer("cat"));

  ChainedByteRangeHead queue(std::move(buf));
  checkConsistency(queue);

  ChainedByteRangeHead queue2(std::move(queue));
  checkConsistency(queue2);
}

TEST(ChainedByteRangeHead, TestSplitAtMostRemoveFirstChunk) {
  auto buf = folly::IOBuf::copyBuffer("jars");
  buf->appendToChain(folly::IOBuf::copyBuffer("apple"));
  buf->appendToChain(folly::IOBuf::copyBuffer("ball"));

  ChainedByteRangeHead queue(buf);
  checkConsistency(queue);

  auto prefix = queue.splitAtMost(4);
  EXPECT_EQ(prefix.chainLength(), 4);
  EXPECT_EQ(queue.chainLength(), 9);
}

TEST(ChainedByteRangeHead, TestSplitAtMostRemoveAllExceptLast) {
  auto buf = folly::IOBuf::copyBuffer("jars");
  buf->appendToChain(folly::IOBuf::copyBuffer("apple"));
  buf->appendToChain(folly::IOBuf::copyBuffer("ball"));

  ChainedByteRangeHead queue(buf);
  checkConsistency(queue);

  auto prefix = queue.splitAtMost(9);
  EXPECT_EQ(prefix.chainLength(), 9);
  EXPECT_EQ(queue.chainLength(), 4);
}
