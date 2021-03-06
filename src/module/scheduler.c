#include <stdlib.h>
#include <string.h>

#include "uv.h"

#include "scheduler.h"
#include "wren.h"
#include "vm.h"

// A handle to the "Scheduler" class object. Used to call static methods on it.
static WrenValue* schedulerClass;

// This method resumes a fiber that is suspended waiting on an asynchronous
// operation. The first resumes it with zero arguments, and the second passes
// one.
static WrenValue* resume1;
static WrenValue* resume2;
static WrenValue* resumeError;

static void resume(WrenValue* method)
{
  WrenInterpretResult result = wrenCall(getVM(), method);
  
  // If a runtime error occurs in response to an async operation and nothing
  // catches the error in the fiber, then exit the CLI.
  if (result == WREN_RESULT_RUNTIME_ERROR)
  {
    uv_stop(getLoop());
    setExitCode(70); // EX_SOFTWARE.
  }
}

void schedulerCaptureMethods(WrenVM* vm)
{
  wrenEnsureSlots(vm, 1);
  wrenGetVariable(vm, "scheduler", "Scheduler", 0);
  schedulerClass = wrenGetSlotValue(vm, 0);
  
  resume1 = wrenMakeCallHandle(vm, "resume_(_)");
  resume2 = wrenMakeCallHandle(vm, "resume_(_,_)");
  resumeError = wrenMakeCallHandle(vm, "resumeError_(_,_)");
}

void schedulerResume(WrenValue* fiber, bool hasArgument)
{
  WrenVM* vm = getVM();
  wrenEnsureSlots(vm, 2 + (hasArgument ? 1 : 0));
  wrenSetSlotValue(vm, 0, schedulerClass);
  wrenSetSlotValue(vm, 1, fiber);
  wrenReleaseValue(vm, fiber);
  
  // If we don't need to wait for an argument to be stored on the stack, resume
  // it now.
  if (!hasArgument) resume(resume1);
}

void schedulerFinishResume()
{
  resume(resume2);
}

void schedulerResumeError(WrenValue* fiber, const char* error)
{
  schedulerResume(fiber, true);
  wrenSetSlotString(getVM(), 2, error);
  resume(resumeError);
}

void schedulerShutdown()
{
  // If the module was never loaded, we don't have anything to release.
  if (schedulerClass == NULL) return;
  
  WrenVM* vm = getVM();
  wrenReleaseValue(vm, schedulerClass);
  wrenReleaseValue(vm, resume1);
  wrenReleaseValue(vm, resume2);
  wrenReleaseValue(vm, resumeError);
}
