/**
 * @file	threadbase.h
 * @brief	スレッド基底クラスの宣言およびインターフェイスの定義をします
 */

#pragma once

#include <pthread.h>

/**
 * @brief スレッド基底クラス
 */
class CThreadBase
{
public:
	CThreadBase();
	virtual ~CThreadBase();

	bool Start();
	void Stop();

	/**
	 * スタック サイズの設定
	 * @param[in] stack_size スタック サイズ
	 */
	void SetStackSize(size_t stack_size) { m_stack_size = stack_size; }

protected:
	virtual bool Task()=0;		//!< スレッド タスク

private:
	pthread_t m_thread;			//!< スレッド フラグ
	bool m_bCreated;			//!< スレッド作成フラグ
	bool m_bDone;				//!< 終了フラグ
	size_t m_stack_size;		//!< スタック サイズ
	static void* StartRoutine(void* arg);
};
