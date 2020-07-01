#include "gtest/gtest.h"
#include "solmsg.h"

 class SolMsgTest: public ::testing::Test{
  protected:
  void SetUp() override {

  }
  SolMsg msg;
};
TEST_F(SolMsgTest, Init){
  EXPECT_NE(msg.ptr(), nullptr);
}
TEST_F(SolMsgTest, Attatchment){
  const std::string s("test body.");
  msg.setBinaryAttachment(s);
  std::string res = msg.getBinaryAttachmentString();
  EXPECT_EQ(res, s);
}
