#include "gtest/gtest.h"
#include "session.h"
#include "shioaji.h"
#include "gmock/gmock.h"

using namespace shioaji;
using namespace shioaji::account;

//class MockSession: public Session {
// public:
//  MOCK_METHOD(SolMsg, _login, (const json & payload), ());
//};

class MockCSol: public CSol{
 public:
//  MOCK_METHOD()
};

class SolaceSessionTest : public ::testing::Test {
 protected:
  virtual void SetUp() override {
//    sess = new shioaji::Session("203.66.91.161:80",
//                                "sinopac",
//                                "shioaji",
//                                "sahjiio111");
//    MockCSol sol = MockCSol();
//    sess = new shioaji::Session(sol);
  }
  virtual void TearDown() override {
//    delete sess;
  }
  shioaji::Session *sess;

};

TEST_F(SolaceSessionTest, Login) {
//  std::vector<Account> accounts = sess->Login("P124081046", "deep3051deep");
//  EXPECT_NE(accounts.size(), 0);
}

