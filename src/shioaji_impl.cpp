//
// Created by 011295 on 2020/6/17.
//
#include "shioaji_impl.h"

namespace shioaji {

Shioaji::Shioaji() : sess(new Session("203.66.91.161:80",
                                      "sinopac",
                                      "shioaji",
                                      "sahjiio111")) {

}

Shioaji::~Shioaji() { delete sess; }

bool Shioaji::login(const std::string &person_id, const std::string &password) {
  return true;
}
}
