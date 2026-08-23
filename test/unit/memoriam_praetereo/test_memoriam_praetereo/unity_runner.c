
#include "unity.h"
#include "memoriam_praetereo/memoriam_praetereo.h"

extern void setUp(void);
extern void tearDown(void);
extern void test_dma_header_is_self_contained(void);


static void CMock_Init(void)
{
}
static void CMock_Verify(void)
{
}
static void CMock_Destroy(void)
{
}

void setUp(void) {}

void tearDown(void) {}

void resetTest(void);
void resetTest(void)
{
  tearDown();
  CMock_Verify();
  CMock_Destroy();
  CMock_Init();
  setUp();
}
void verifyTest(void);
void verifyTest(void)
{
  CMock_Verify();
}

static void run_test(UnityTestFunction func, const char* name, UNITY_LINE_TYPE line_num)
{
    Unity.CurrentTestName = name;
    Unity.CurrentTestLineNumber = (UNITY_UINT) line_num;
#ifdef UNITY_USE_COMMAND_LINE_ARGS
    if (!UnityTestMatches())
        return;
#endif
    Unity.NumberOfTests++;
    UNITY_CLR_DETAILS();
    UNITY_EXEC_TIME_START();
    CMock_Init();
    if (TEST_PROTECT())
    {
        setUp();
        func();
    }
    if (TEST_PROTECT())
    {
        tearDown();
        CMock_Verify();
    }
    CMock_Destroy();
    UNITY_EXEC_TIME_STOP();
    UnityConcludeTest();
}

int main(void)
{
  UnityBegin("C:/Users/Douglas/Desktop/git_project/mmgrwork/MMgr/test/unit/memoriam_praetereo/test_memoriam_praetereo\\test_memoriam_praetereo.c");
  run_test(test_dma_header_is_self_contained, "test_dma_header_is_self_contained", 8);

  return UNITY_END();
}
