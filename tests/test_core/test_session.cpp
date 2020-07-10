#include "gtest/gtest.h"
#include "session.h"

class SolaceSessionTest : public ::testing::Test {
 protected:
  virtual void SetUp() override {
    sess = new shioaji::Session("203.66.91.161:80", "sinopac", "shioaji", "sahjiio111");
  }
  virtual void  TearDown() override {
    delete sess;
  }
  shioaji::Session * sess;
};

TEST_F(SolaceSessionTest, Login) {
  bool res = sess->Login("PAPIUSER01", "2222");
  EXPECT_EQ(res, false);
}

