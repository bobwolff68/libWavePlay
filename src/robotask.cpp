// Copyright 2022 Robert M. Wolff (bob dot wolff 68 at gmail dot com)
//
// Redistribution and use in source and binary forms, with or without modification, 
// are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this 
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation and/or 
// other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors 
// may be used to endorse or promote products derived from this software without 
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
#include "robotask.h"

// #define _TASKDEBUG

RoboTask::RoboTask(const char* taskName, uint8_t priority, int stacksize) {
  char _name[128];

  if (!taskName)
  {
      // Modified task name default. FreeRTOS for ESP32 has a max length of 16
      // and the millis function could return big numbers if tasks are not all created at startup.
#ifdef ESP_PLATFORM
    sprintf(_name, "roboTsk%ld", millis());
#endif
  }
  else
    strcpy(_name, taskName);

  enabled_ = false;
  bConfirmedPaused = false;
  running_ = true;
  isDead_ = false;
  runDelayPeriod = 20; // 20ms default delay between calling Run() - can be modified by user.

#ifdef ESP_PLATFORM
  xTaskCreate(
    &RoboTask::RoboPrivateStarterTask,  
    _name,            // A name just for humans
    stacksize,        // This stack size can be checked & adjusted by reading the Stack Highwater
    (void*)this,      //Parameters passed to the task function
    priority,         // Priority, (configMAX_PRIORITIES-1) being the highest, and 0 being the lowest.
    &Task_Handler );  //Task handle
  startTimer = millis();
#else
  pThread = new std::thread(&RoboTask::RoboPrivateStarterTask, this);
  startTimePoint = std::chrono::high_resolution_clock::now();
#endif
}

RoboTask::~RoboTask(){
  // Waits for final delay period and Run() to complete
	this->Terminate();
#ifndef ESP_PLATFORM
  pThread->join();
#endif
}

void RoboTask::setBaseRunDelay(uint32_t delay) {
  runDelayPeriod = delay;
}

bool RoboTask::hasElapsed(unsigned long elapsed) {
#ifdef ESP_PLATFORM
    return millis() > startTimer + elapsed;
#else
    static std::chrono::high_resolution_clock::time_point curPT;
    std::chrono::duration<double, std::milli> span;
 
    curPT = std::chrono::high_resolution_clock::now();
    span = curPT - startTimePoint;
    return (unsigned long)span.count() > elapsed;
#endif
}

void RoboTask::resetElapsedTimer() {
#ifdef ESP_PLATFORM
    startTimer = millis();
#else
    startTimePoint = std::chrono::high_resolution_clock::now();
#endif
}

uint16_t RoboTask::getStackHighWaterMark() {
//    Serial.print("- TASK ");
//    Serial.print(pcTaskGetName(taskBlinkHandle)); // Get task name with handler
//    Serial.print(", High Watermark: ");
#ifdef ESP_PLATFORM
    return Task_Handler ? uxTaskGetStackHighWaterMark(Task_Handler) : 0;
#else
    return 0;
#endif
//    Serial.println();
}

bool RoboTask::isThisThreadContext() {
#ifdef ESP_PLATFORM
  if (Task_Handler == xTaskGetCurrentTaskHandle())
    return true;
#else
  if (this_thread_id == std::this_thread::get_id())
    return true;
#endif
  return false;
}

void RoboTask::RoboPrivateStarterTask(void* vtask) {
#ifdef _TASKDEBUG
  unsigned long roundTrips = 0;
#endif
	RoboTask*task= (RoboTask*)vtask;

  // Setup who the current running task is in native code
#ifndef ESP_PLATFORM
  task->this_thread_id = std::this_thread::get_id();
#endif

  while (task->running_) {

#ifdef _TASKDEBUG
    if (Serial) {
      Serial.print("RoboTask:: Round trips are ");
      Serial.print(roundTrips++);
      Serial.print(" Enabled=");
      Serial.print(task->enabled_);
      Serial.print(" Running=");
      Serial.println(task->running_);
    }
#endif
    if (task->enabled_) {
      task->bConfirmedPaused = false;
      task->Run();
#ifdef ESP_PLATFORM
      if (task->runDelayPeriod)
        vTaskDelay( task->runDelayPeriod / portTICK_PERIOD_MS ); // Only wait 20ms when task is active.
#else
      if (task->runDelayPeriod)
        std::this_thread::sleep_for(std::chrono::milliseconds(task->runDelayPeriod));
#endif
    }
    else {
      task->bConfirmedPaused = true;
#ifdef ESP_PLATFORM
      vTaskDelay( 50 / portTICK_PERIOD_MS ); // 50 ms wait period while task is 'paused'
#else
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
#endif
    }
  }
  task->isDead_ = true; // Falling off the edge of the earth...

  // Now the task itself can be deleted.
#ifdef ESP_PLATFORM
//  Serial.println("***DELETING TASK***");
  vTaskDelete(NULL);
#else
  // In non-ESP32-land, the destructor has to delete the thread by join() but that means we have to exit here. Not wait forever.
  return;
#endif
//  Serial.println("***TASK DELETED***");
  while(1)
#ifdef ESP_PLATFORM
    vTaskDelay( 500 / portTICK_PERIOD_MS ); // Task is DEAD. 500 ms wait period not necessary.
#else
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
#endif

  // FreeRTOS documentation says the task must *NOT* return. It must die.
  // It should have died or gone into an infinite loop and will never get here.
  return;
}

void RoboTask::Start() {
  enabled_ = true;
}

void RoboTask::Pause() {
  enabled_ = false;

  if (isThisThreadContext()) {
#ifdef ESP_PLATFORM
    Serial.println("RoboTask - SELF-Pause - not waiting for confirmation to happen.");
#else
    printf("RoboTask - SELF-Pause - not waiting for confirmation to happen.\n");
#endif
    return;
  }

  // CONFIRM that we're paused - this allows for the final 'Run()' cycle to finish.
  while (!bConfirmedPaused) {
#ifdef ESP_PLATFORM
    vTaskDelay( 50 / portTICK_PERIOD_MS );
#else
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
#endif
  }
}

bool RoboTask::isDead() {
  return !running_;
}

bool RoboTask::isPaused() {
  return bConfirmedPaused;
}

void RoboTask::Terminate() {
  running_ = false;

  if (isThisThreadContext()) {
#ifdef ESP_PLATFORM
    Serial.println("RoboTask - SELF-Termination - not waiting for isDead_ to happen.");
#else
    printf("RoboTask - SELF-Termination - not waiting for isDead_ to happen.\n");
#endif
    return;
  }

  // CONFIRM that we're paused - this allows for the final 'Run()' cycle to finish.
  while (!isDead_){
#ifdef ESP_PLATFORM
    vTaskDelay( 50 / portTICK_PERIOD_MS );
#else
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
#endif
  }
  // Cannot wait around for isDead_ as we used to.
  // Otherwise there's no way to self terminate.
  // So now termination should happen 'soon' depending on the delay in Run()

  return;
}
