/*******************************************************************************
Description:  Implements generic register and register file classes.
Author:       Ghassan Shobaki
Created:      Jun. 2010
Last Update:  Mar. 2011
*******************************************************************************/

#ifndef OPTSCHED_BASIC_REGISTER_H
#define OPTSCHED_BASIC_REGISTER_H

#include "llvm/CodeGen/OptSched/generic/defines.h"
#include "llvm/CodeGen/OptSched/generic/bit_vector.h"

namespace opt_sched {

// Represents a a single register of a certain type and tracks the number of
// times this register is defined and used.
class Register {
  public:
    Register(int16_t type = 0, int num = 0, int physicalNumber = INVALID_VALUE);

    int16_t GetType() const;
    void SetType(int16_t type);

    int GetNum() const;
    void SetNum(int num);

    bool IsPhysical() const;
    int GetPhysicalNumber() const;
    void SetPhysicalNumber(int physicalNumber);

    void AddUse();
    int GetUseCnt() const;
    int GetCrntUseCnt() const;

    void AddDef();
    int GetDefCnt() const;

    void AddCrntUse();
    void DelCrntUse();
    void ResetCrntUseCnt();

    void IncrmntCrntLngth();
    void DcrmntCrntLngth();
    void ResetCrntLngth(); 
    int GetCrntLngth() const;

    bool IsLive() const;

    const Register& operator= (Register& rhs);

    void SetupConflicts(int regCnt);
    void ResetConflicts();
    void AddConflict(int regNum, bool isSpillCnddt);
    int GetConflictCnt() const;
    bool IsSpillCandidate() const;
    

  private:
    int16_t type_;
    int num_;
    int defCnt_;
    int useCnt_;
    int crntUseCnt_;
    int crntLngth_;
    int physicalNumber_;
    BitVector conflicts_;
    bool isSpillCnddt_;
    
};

// Represents a file of registers of a certain type and tracks their usages.
class RegisterFile {
  public:
    RegisterFile();
    ~RegisterFile();

    int GetRegCnt() const;
    void SetRegCnt(int regCnt);

    int16_t GetRegType() const;
    void SetRegType(int16_t regType);

    Register* GetReg(int num) const;
    Register* FindLiveReg(int physNum) const;

    void ResetCrntUseCnts();
    void ResetCrntLngths();

   int FindPhysRegCnt();
   int GetPhysRegCnt() const;

   void SetupConflicts();
   void ResetConflicts();
   void AddConflictsWithLiveRegs (int regNum, int liveRegCnt);
   int GetConflictCnt();

  private:
    int16_t regType_;
    int regCnt_;
    int physRegCnt_;
    Register* regs_;
};

} // end namespace opt_sched

#endif
