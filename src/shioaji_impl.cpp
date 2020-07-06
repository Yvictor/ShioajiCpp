//
// Created by 011295 on 2020/6/17.
//
#include "shioaji.h"
#include "shioaji_impl.h"

namespace shioaji {

Shioaji::Shioaji() : sess(new Session()) {

}

Shioaji::~Shioaji() { delete sess; }

bool Shioaji::login(const std::string &person_id, const std::string &password) {
  return true;
}
}
