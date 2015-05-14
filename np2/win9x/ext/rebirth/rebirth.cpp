/**
 * @file	rebirth.cpp
 * @brief	SCCI アクセス クラスの動作の定義を行います
 */

#include "compiler.h"
#include "rebirth.h"
#include "scci.h"
#include "SCCIDefines.h"

/**
 * コンストラクタ
 */
CRebirth::CRebirth()
	: m_hModule(NULL)
	, m_pManager(NULL)
	, m_pChip(NULL)
{
}

/**
 * デストラクタ
 */
CRebirth::~CRebirth()
{
}

/**
 * 初期化
 * @retval true 成功
 * @retval false 失敗
 */
bool CRebirth::Initialize()
{
	Deinitialize();

	m_hModule = ::LoadLibrary(TEXT("SCCI.DLL"));
	if (m_hModule == NULL)
	{
		return false;
	}

	do
	{
		// サウンドインターフェースマネージャー取得用関数アドレス取得
		SCCIFUNC fnGetSoundInterfaceManager = reinterpret_cast<SCCIFUNC>(::GetProcAddress(m_hModule, "getSoundInterfaceManager"));
		if (fnGetSoundInterfaceManager == NULL)
		{
			break;
		}

		// サウンドインターフェースマネージャー取得
		m_pManager = (*fnGetSoundInterfaceManager)();
		if (m_pManager == NULL)
		{
			break;
		}

		// サウンドインターフェースマネージャーインスタンス初期化
		// 必ず最初に実行してください
		if (!m_pManager->initializeInstance())
		{
			break;
		}

		// リセットを行う
		Reset();

		m_pChip = m_pManager->getSoundChip(SC_TYPE_YM2608, SC_CLOCK_7987200);
		if (m_pChip != NULL)
		{
			// サウンドチップ取得できた
			printf("Found YM2608\n");
			return true;
		}
	} while (false /*CONSTCOND*/);

	Deinitialize();
	return false;
}

/**
 * 解放
 */
void CRebirth::Deinitialize()
{
	if (m_pManager)
	{
		if (m_pChip)
		{
			// チップの開放（チップ単位で開放の場合）
			m_pManager->releaseSoundChip(m_pChip);
		}

		// 一括開放する場合（チップ一括開放の場合）
		m_pManager->releaseAllSoundChip();

		// サウンドインターフェースマネージャーインスタンス開放
		// FreeLibraryを行う前に必ず呼び出ししてください
		m_pManager->releaseInstance();
	}

	if (m_hModule)
	{
		::FreeLibrary(m_hModule);
	}
	m_hModule = NULL;
	m_pManager = NULL;
	m_pChip = NULL;
}

/**
 * ビジー?
 * @retval true ビジー
 * @retval false レディ
 */
bool CRebirth::IsBusy()
{
	return (m_pChip == NULL);
}

/**
 * 音源リセット
 */
void CRebirth::Reset()
{
	if (m_pManager)
	{
		// リセットを行う
		m_pManager->reset();
	}
}

/**
 * レジスタ書き込み
 * @param[in] nAddr アドレス
 * @param[in] cData データ
 */
void CRebirth::WriteRegister(UINT nAddr, UINT8 cData)
{
	if (m_pChip)
	{
		m_pChip->setRegister(nAddr, cData);
	}
}
