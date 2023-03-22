#include <iostream>
#include "org_brain4free_jprojectm_ProjectM.h"

JNIEXPORT void JNICALL Java_org_brain4free_jprojectm_ProjectM_sayHello
  (JNIEnv* env, jobject thisObject) {
    std::cout << "Hello from C++ !!" << std::endl;
}
