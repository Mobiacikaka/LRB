#include "user.hpp"

#include <iostream>
#include <thread>
#include <ctime>
#include <mutex>

User::User()
{

}

User::~User()
{

}

int User::MemoryWindowCaching()
{
// while receiving request
//  if mtx_mw.try_lock
//   record request in memory window
//   mtx.unlock
}

int User::UnlabledDatasetSampling()
{
// while true:
//  sleep(time)
//  if mtx_mw.try_lock
//   sample randomly // ? how much 
//   mtx_mw.unlock
//   record the features
//    if mtx_ud.try_lock
//     record request in unlabled dataset
//     mtx_ud.unlock
}

int User::DatasetLabling()
{
// while true:
//  if mtx_ud.try_lock
//   take object in unlabled dataset
//   mtx_ud.unlock()
//  lable object
//  !if mtx_ld.try_lock and mtx_ld_amountexceed.try_lock
//   !record object in labled dataset
//   !if lab
}