//
// Created by 011295 on 2020/6/17.
//
#include <shioaji.h>

#include "shioaji_impl.h"

namespace shioaji {

Shioaji::Shioaji(bool simulation) {
  if (simulation) {
    sess =
        new Session("218.32.76.102:80", "sinopac", "shioaji", "shioaji111");
  } else {
    sess = new Session("203.66.91.161:80",
                       "sinopac",
                       "shioaji",
                       "sahjiio111");
  }
}

Shioaji::Shioaji(Session *session) : sess(session) {}

Shioaji::~Shioaji() { delete sess; }

std::vector<account::Account> Shioaji::login(const std::string &person_id,
                                             const std::string &password) {
  return sess->Login(person_id, password);
}

}
