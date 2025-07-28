#include "PromptGenerators.h"
#include <algorithm>
#include <QDate>
#include <QDateTime>

namespace PromptGenerators
{

  QString generateToolDiscoveryPrompt(const QString &userQuery,
                                      const QVector<ToolInfo> &tools)
  {
    QStringList toolsInfo;
    for (const auto &tool : tools)
    {
      QString toolStr = QString("• %1: %2").arg(tool.name, tool.description);
      if (!tool.examples.isEmpty())
      {
        QStringList examples = tool.examples.mid(0, 3);
        toolStr += QString("\n  예시: %1").arg(examples.join(", "));
      }
      toolsInfo.append(toolStr);
    }

    // DB 도구 확인
    bool hasDbTools = std::any_of(tools.begin(), tools.end(),
                                  [](const ToolInfo &tool)
                                  { return tool.name.startsWith("db_"); });

    // 디바이스 제어 도구 확인
    bool hasDeviceControl = std::any_of(tools.begin(), tools.end(),
                                        [](const ToolInfo &tool)
                                        { return tool.name == "device_control"; });

    // MQTT 도구 확인
    bool hasMqttDeviceControl = std::any_of(tools.begin(), tools.end(),
                                            [](const ToolInfo &tool)
                                            { return tool.name == "mqtt_device_control"; });

    bool hasFailureStats = std::any_of(tools.begin(), tools.end(),
                                       [](const ToolInfo &tool)
                                       { return tool.name == "conveyor_failure_stats"; });

    bool hasDeviceStats = std::any_of(tools.begin(), tools.end(),
                                      [](const ToolInfo &tool)
                                      { return tool.name == "device_statistics"; });

    QString contextInfo;

    // 현재 날짜 정보 계산
    QDate currentDate = QDate::currentDate();
    int currentYear = currentDate.year();
    int currentMonth = currentDate.month();
    int currentDay = currentDate.day();

    if (hasDbTools)
    {
      contextInfo += QString(R"(
특별 참고사항 - 데이터베이스:
- 로그, 데이터, 통계, 분석, 정보 관련 질문은 db_find 도구를 사용합니다
- 현재 날짜: %1년 %2월 %3일
- 시간/날짜 관련 로그 조회는 db_count나 db_find를 사용합니다
- 데이터베이스 구조나 정보는 db_info를 사용합니다
- 에러, 오류 관련 통계도 db_find를 사용합니다

디바이스 이름 매핑:
- 컨베이어1, 컨베이어 1 → conveyor_01
- 컨베이어2, 컨베이어 2 → conveyor_02  
- 컨베이어3, 컨베이어 3 → conveyor_03
- 피더1, 피더 1 → feeder_01
- 피더2, 피더 2 → feeder_02
- 로봇팔, 로봇암, 로봇 → robot_arm_01

로그 표시 규칙:
- 날짜 그룹핑: 같은 날짜는 "MM월 DD일 로그" 헤더로 묶어서 표시
- 시간 형식: 헤더 이후에는 "HH시 MM분" 형식만 사용
- 번호 제거: 맨 앞 번호(1~10) 없이 표시
- UNKNOWN 로그 → "🔴 SPD 에러"로 표시
- 상태 이모지: ✅정상, ⚠️경고, 🔴에러, ⚪SPD에러
- 예시 형식:
  ```
  📅 7월 28일 로그
  12시 2분 | 🔴 SPD 에러 - 컨베이어1 속도 이상
  12시 1분 | ✅ 정상 - 피더1 작동 중
  11시 59분 | ⚠️ 경고 - 로봇팔 온도 상승
  ```

특별 케이스:
- "컨베이어1 6월 정보" → db_find (device_id: conveyor_01, 6월 범위)
- "컨베이어1 오늘 정보" → db_find (device_id: conveyor_01, 오늘 범위)
- "이번달 에러 통계" → db_find (level: error+UNKNOWN, %4월 범위)

날짜 관련 처리:
- "6월": %1-06-01부터 %1-06-30까지
- "7월": %1-07-01부터 %1-07-31까지
- "이번달": %1-%2-01부터 현재까지
- "오늘": %1-%2-%3 하루 전체
)").arg(currentYear).arg(currentMonth, 2, 10, QChar('0')).arg(currentDay, 2, 10, QChar('0')).arg(currentMonth);
    }

    if (hasDeviceControl)
    {
      contextInfo += R"(
특별 참고사항 - 디바이스 제어 (HTTP):
- 켜다, 시작, 가동, 작동, 켜줘 → "on" 명령
- 끄다, 정지, 멈추다, 중지, 꺼줘 → "off" 명령
- 디바이스 타입: 컨베이어(3대), 피더(2대), 로봇팔(1대)
- "모든", "전체", "전부" 키워드가 있으면 해당 타입의 모든 디바이스 제어

특별 케이스:
- "피더1 켜줘" → device_control (device_id: feeder_01, command: on)
- "피더1 꺼줘" → device_control (device_id: feeder_01, command: off)
)";
    }

    if (hasMqttDeviceControl)
    {
      contextInfo += R"(
특별 참고사항 - MQTT 디바이스 제어:
- MQTT를 통한 실시간 디바이스 제어
- 제어 가능 디바이스와 토픽:
  * 피더 2번, 피더02: feeder_02/cmd
  * 컨베이어 2번, 컨베이어02: factory/conveyor_02/cmd
  * 컨베이어 3번, 컨베이어03: conveyor_03/cmd
  * 로봇팔, 로봇암: robot_arm_01/cmd
- 명령어: "켜", "꺼", "시작", "정지" → on/off
- "전체", "모든" 키워드 시 모든 기기 제어
)";
    }

    if (hasFailureStats || hasDeviceStats)
    {
      contextInfo += R"(
특별 참고사항 - 통계 데이터:
- 불량률, 양품, 불량품, 생산량 관련 → conveyor_failure_stats 사용
- 속도, 평균 속도, 운영 통계, 장비 통계 → device_statistics 사용  
- 시간 범위 지정 가능: "오늘", "지난 1시간", 특정 날짜 등
- 실시간 캐시된 데이터 조회 가능
- 캐시된 데이터 없으면 "데이터가 없습니다" 메시지 출력

통계 요청 키워드:
- "통계", "속도", "평균", "성능", "운영 상태" → device_statistics
- "불량률", "불량품", "양품", "생산량", "품질" → conveyor_failure_stats

예시:
- "컨베이어1 속도 통계" → device_statistics (device_id: "conveyor_01")
- "피더1 운영 통계" → device_statistics (device_id: "feeder_01")
- "컨베이어 불량률" → conveyor_failure_stats (device_id: "conveyor_01")
)";
    }

    // 특별 케이스 처리
    QString specialCaseResponse = handleSpecialCases(userQuery, tools);
    if (!specialCaseResponse.isEmpty())
    {
      return specialCaseResponse;
    }

    QString prompt = QString(R"(당신은 스마트 팩토리 시스템의 AI 어시스턴트입니다. 사용자의 요청을 정확히 분석하고 적절한 도구를 선택해주세요.

사용자 요청: "%1"

사용 가능한 도구:
%2

=== 간단한 분석 가이드 ===

🔍 **요청 분류 및 도구 선택:**

A) 🤖 **기능 소개** ("어떤 기능", "뭘 할 수 있", "도움말" 등)
   ➜ 적합한 도구: 없음
   ➜ 응답: 미리 정의된 기능 소개 메시지

B)  **장비 제어** ("켜", "꺼", "시작", "정지" + 장비명)
   ➜ 피더2/컨베이어2,3/로봇팔: MQTT 장비 제어
   ➜ 나머지: 장비 제어

C) 📊 **데이터 조회** ("정보", "로그", "확인", "보여", "통계", "분석" 등)
   ➜ 모든 데이터 조회: 데이터베이스 조회

**응답 형식:**
1. 요청 분석: [간단한 의도 파악]
2. 적합한 도구: [한글 도구명 또는 "없음"]
3. 이유: [선택 근거]
4. 응답: [사용자 메시지]

%3)")
                         .arg(userQuery)
                         .arg(toolsInfo.join("\n"))
                         .arg(contextInfo);

    return prompt;
  }

  QString generateToolExecutionPrompt(ConversationContext *context)
  {
    auto toolIt = std::find_if(context->availableTools.begin(), context->availableTools.end(),
                               [context](const ToolInfo &tool)
                               {
                                 return tool.name == context->selectedTool.value();
                               });

    if (toolIt == context->availableTools.end())
    {
      return "";
    }

    const ToolInfo &tool = *toolIt;

    // 대화 맥락 구성
    QString conversationContext;
    int startIdx = std::max(0, static_cast<int>(context->conversationHistory.size()) - 3);
    for (int i = startIdx; i < context->conversationHistory.size(); ++i)
    {
      const auto &msg = context->conversationHistory[i];
      conversationContext += QString("%1: %2\n").arg(msg.role, msg.content);
    }

    // 도구별 특별 지침
    QString specialInstructions = getSpecialInstructions(context->selectedTool.value());

    QString prompt = QString(R"(이전 대화 맥락:
%1

현재 사용자 요청: "%2"

선택된 도구: %3
도구 설명: %4

입력 스키마:
%5
%6
이 도구를 사용하여 사용자의 요청을 처리하기 위한 매개변수를 JSON 형식으로 생성해주세요.
반드시 유효한 JSON만 응답하고, 다른 설명은 포함하지 마세요.

예시 응답 형식:
{"매개변수_이름": "값"})")
                         .arg(conversationContext)
                         .arg(context->userQuery)
                         .arg(tool.name)
                         .arg(tool.description)
                         .arg(QString::fromUtf8(QJsonDocument(tool.inputSchema).toJson(QJsonDocument::Indented)))
                         .arg(specialInstructions);

    return prompt;
  }

  QString getSpecialInstructions(const QString &toolName)
  {
    // 현재 날짜 정보 계산
    QDate currentDate = QDate::currentDate();
    int currentYear = currentDate.year();
    int currentMonth = currentDate.month();
    int currentDay = currentDate.day();
    
    // 어제 날짜 계산
    QDate yesterday = currentDate.addDays(-1);
    int yesterdayDay = yesterday.day();
    
    if (toolName == "db_find")
    {
      return QString(R"(
특별 지침 - 로그 조회:
- collection이 명시되지 않았으면 "logs_all"을 사용하세요
- sort는 항상 {"timestamp": -1}로 설정 (최신순)
- limit 규칙:
  * "최근", "요일" 관련: 10
  * "월" 관련: 20
  * "모든", "전체": 100
  * "N개" 명시: N (최대 100)
- 날짜 관련 (현재 날짜: %1-%2-%3 기준):
  * "6월": {"timestamp": {"$gte": "%1-06-01T00:00:00Z", "$lt": "%1-07-01T00:00:00Z"}}
  * "7월": {"timestamp": {"$gte": "%1-07-01T00:00:00Z", "$lt": "%1-08-01T00:00:00Z"}}
  * "이번달": {"timestamp": {"$gte": "%1-%4-01T00:00:00Z", "$lt": "%1-%5-01T00:00:00Z"}}
  * "오늘": {"timestamp": {"$gte": "%1-%4-%6T00:00:00Z", "$lt": "%1-%4-%7T00:00:00Z"}}
  * "어제": {"timestamp": {"$gte": "%1-%4-%8T00:00:00Z", "$lt": "%1-%4-%6T00:00:00Z"}}

디바이스명 변환:
- "컨베이어1", "컨베이어 1", "컨베이어01" → "conveyor_01"
- "피더1", "피더 1", "피더01" → "feeder_01"
- "피더2", "피더 2", "피더02" → "feeder_02"

로그 데이터 포맷팅 규칙:
- 날짜 그룹핑: 같은 날짜의 로그들을 묶어서 표시
- 헤더 형식: "📅 MM월 DD일 로그" (한 번만 표시)
- 시간 형식: "HH시 MM분" (날짜 없이)
- 번호 제거: 1, 2, 3... 등의 순서 번호 표시하지 않음
- UNKNOWN 로그는 "🔴 SPD 에러"로 분류 및 표시
- 에러 레벨 매핑: UNKNOWN → ERROR (SPD 관련)
- 예시 응답 형식:
  ```
  📅 7월 28일 로그 (conveyor_01)
  12시 2분 | 🔴 SPD 에러 - 속도 이상 감지
  12시 1분 | ✅ 정상 작동
  11시 59분 | ⚠️ 경고 - 온도 상승
  
  📅 7월 27일 로그 (conveyor_01)  
  23시 58분 | ✅ 정상 작동
  ```

정확한 매핑 예시:
- "컨베이어1 6월 정보 보여줘" → {
    "collection": "logs_all",
    "query": {
      "device_id": "conveyor_01",
      "timestamp": {"$gte": "%1-06-01T00:00:00Z", "$lt": "%1-07-01T00:00:00Z"}
    },
    "sort": {"timestamp": -1},
    "limit": 20
  }
- "컨베이어1 오늘 정보 보여줘" → {
    "collection": "logs_all",
    "query": {
      "device_id": "conveyor_01", 
      "timestamp": {"$gte": "%1-%4-%6T00:00:00Z", "$lt": "%1-%4-%7T00:00:00Z"}
    },
    "sort": {"timestamp": -1},
    "limit": 20
  }
- "이번달 에러" 관련 → {
    "collection": "logs_all",
    "query": {
      "$or": [
        {"level": "error"},
        {"level": "UNKNOWN"},
        {"message": {"$regex": "UNKNOWN", "$options": "i"}}
      ],
      "timestamp": {"$gte": "%1-%4-01T00:00:00Z", "$lt": "%1-%5-01T00:00:00Z"}
    },
    "sort": {"timestamp": -1},
    "limit": 20
  }

응답 포맷팅:
- 날짜 그룹핑으로 표시 (같은 날짜는 한 번만 헤더 표시)
- 형식: "📅 MM월 DD일 로그" → "HH시 MM분 | 상태 | 내용"
- 번호(1, 2, 3...) 표시하지 않음
- UNKNOWN 로그는 "🔴 SPD 에러"로 표시
- 각 로그에 명확한 시간과 상태 정보 포함
- 예시:
  ```
  📅 7월 28일 로그 (conveyor_01)
  12시 2분 | 🔴 SPD 에러 - 속도 이상
  12시 1분 | ✅ 정상 작동
  11시 59분 | ⚠️ 경고 - 온도 상승
  ```
)").arg(currentYear)
    .arg(currentMonth, 2, 10, QChar('0'))
    .arg(currentDay, 2, 10, QChar('0'))
    .arg(currentMonth, 2, 10, QChar('0'))
    .arg(currentMonth + 1, 2, 10, QChar('0'))
    .arg(currentDay, 2, 10, QChar('0'))
    .arg(currentDay + 1, 2, 10, QChar('0'))
    .arg(yesterdayDay, 2, 10, QChar('0'));
    }
    else if (toolName.startsWith("db_"))
    {
      return QString(R"(
특별 지침 - 데이터베이스:
- collection이 명시되지 않았으면 "logs_all"을 사용하세요
- 날짜는 반드시 "YYYY-MM-DDTHH:mm:ssZ" 형식으로 지정하세요
- limit이 명시되지 않았으면 20을 사용하세요
- query_type이나 aggregate_type은 정확히 지정된 값 중 하나를 사용하세요

날짜 범위 (현재: %1-%2-%3 기준):
- "6월": {"$gte": "%1-06-01T00:00:00Z", "$lt": "%1-07-01T00:00:00Z"}
- "이번달": {"$gte": "%1-%2-01T00:00:00Z", "$lt": "%1-%4-01T00:00:00Z"}
- "오늘": {"$gte": "%1-%2-%3T00:00:00Z", "$lt": "%1-%2-%5T00:00:00Z"}

디바이스명 변환 규칙:
- "컨베이어1", "컨베이어 1번" → "conveyor_01"
- "피더1", "피더 1번" → "feeder_01"
- "로봇팔", "로봇" → "robot_arm_01"

에러 및 UNKNOWN 로그 처리:
- "이번달 에러 통계" → {
    "collection": "logs_all",
    "query": {
      "$or": [
        {"level": "error"},
        {"level": "ERROR"},
        {"level": "UNKNOWN"},
        {"message": {"$regex": "UNKNOWN|SPD", "$options": "i"}}
      ],
      "timestamp": {"$gte": "%1-%2-01T00:00:00Z", "$lt": "%1-%4-01T00:00:00Z"}
    },
    "limit": 50
  }

응답 포맷팅 지침:
- 날짜 그룹핑: 같은 날짜는 "📅 MM월 DD일 로그" 헤더로 묶음
- 시간 표시: 헤더 이후에는 "HH시 MM분" 형식만 사용
- 번호 제거: 1, 2, 3... 등의 순서 번호 표시하지 않음
- UNKNOWN 로그는 "🔴 SPD 에러"로 표시
- 상태별 이모지: ✅정상, ⚠️경고, 🔴에러, ⚪SPD에러
- 각 항목에 명확한 디바이스명과 상태 표시
- 예시:
  ```
  📅 %2월 %3일 로그
  12시 2분 | 🔴 SPD 에러 - 컨베이어1 이상
  12시 1분 | ✅ 정상 - 피더1 작동
  ```
)").arg(currentYear)
    .arg(currentMonth, 2, 10, QChar('0'))
    .arg(currentDay, 2, 10, QChar('0'))
    .arg(currentMonth + 1, 2, 10, QChar('0'))
    .arg(currentDay + 1, 2, 10, QChar('0'));
    }
    else if (toolName == "device_control")
    {
      return R"(
🔧 **특별 지침 - HTTP 기반 디바이스 제어**

📋 **매개변수 형식:**
- device_id: 정확한 장비 ID 사용
- command: "on" 또는 "off"만 허용

🏭 **장비명 자동 인식 및 변환:**

**컨베이어 시리즈:**
- 표현 예시: "컨베이어1", "컨베이어 1번", "1번 컨베이어", "첫 번째 컨베이어", "컨베이어01"
- 변환 결과: conveyor_01, conveyor_02, conveyor_03

**피더 시리즈:**
- 표현 예시: "피더1", "피더 1번", "1번 피더", "첫 번째 피더", "피더기1"
- 변환 결과: feeder_01
- ⚠️ 주의: 피더2는 mqtt_device_control 사용 필수!

**로봇팔:**
- 표현 예시: "로봇팔", "로봇암", "로봇", "기계팔", "manipulator", "로보트"
- 변환 결과: robot_arm_01

🎛️ **명령어 자동 인식:**

**켜기 명령 (→ "on"):**
- "켜줘", "켜다", "킬", "시작", "가동", "작동", "돌려", "run", "start", "turn on"

**끄기 명령 (→ "off"):**
- "꺼줘", "끄다", "끄", "정지", "멈춰", "중지", "스톱", "stop", "turn off", "shut down"

📝 **정확한 매핑 예시:**

기본 제어:
- "피더1 켜줘" → {"device_id": "feeder_01", "command": "on"}
- "첫 번째 피더 꺼줘" → {"device_id": "feeder_01", "command": "off"}
- "컨베이어 1번 시작" → {"device_id": "conveyor_01", "command": "on"}
- "2번 컨베이어 정지" → {"device_id": "conveyor_02", "command": "off"}
- "로봇팔 가동" → {"device_id": "robot_arm_01", "command": "on"}

자연어 처리:
- "피더기 하나 돌려줘" → {"device_id": "feeder_01", "command": "on"}
- "로봇 멈춰줘" → {"device_id": "robot_arm_01", "command": "off"}
- "세 번째 컨베이어 작동시켜" → {"device_id": "conveyor_03", "command": "on"}

⚠️ **중요 제약사항:**
1. 피더2 제어 요청이면 이 도구를 사용하지 말고 mqtt_device_control 사용
2. 존재하지 않는 장비 ID 생성 금지 (예: conveyor_04, feeder_03 등)
3. command는 반드시 "on" 또는 "off"만 사용
4. 한 번에 하나의 장비만 제어 가능 (여러 장비는 개별 호출 필요)

🤖 **스마트 파싱 가이드:**
- 사용자가 "모든", "전체", "전부" 언급 시 → 해당 타입의 모든 장비 개별 제어
- 애매한 번호 표현도 문맥상 파악 (예: "두 번째" → 02)
- 비표준 표현도 최대한 이해 (예: "콘베어", "휘더" 등의 오타)
)";
    }
     else if (toolName == "mqtt_device_control")
    {
        return R"(
📡 **특별 지침 - MQTT 기반 디바이스 제어**

📋 **매개변수 형식:**
- topic: 장비별 고유 MQTT 토픽
- command: "on" 또는 "off"만 허용

🏭 **장비별 MQTT 토픽 매핑:**

**피더2 (전용!):**
- 표현 예시: "피더2", "피더 2번", "2번 피더", "두 번째 피더", "피더02"
- MQTT 토픽: "feeder_02/cmd"
- ⚠️ 피더2는 이 도구만 사용 가능!

**컨베이어2:**
- 표현 예시: "컨베이어2", "컨베이어 2번", "2번 컨베이어", "두 번째 컨베이어"
- MQTT 토픽: "factory/conveyor_02/cmd"

**컨베이어3:**
- 표현 예시: "컨베이어3", "컨베이어 3번", "3번 컨베이어", "세 번째 컨베이어"
- MQTT 토픽: "conveyor_03/cmd"

**로봇팔:**
- 표현 예시: "로봇팔", "로봇암", "로봇", "기계팔", "manipulator"
- MQTT 토픽: "robot_arm_01/cmd"

🎛️ **명령어 자동 인식:**

**켜기 명령 (→ "on"):**
- "켜", "킬", "켜줘", "시작", "가동", "작동", "돌려", "실행", "run", "start"

**끄기 명령 (→ "off"):**
- "꺼", "끄", "꺼줘", "정지", "멈춰", "중지", "스톱", "중단", "stop", "shutdown"

📝 **정확한 매핑 예시:**

기본 제어:
- "피더2 켜줘" → {"topic": "feeder_02/cmd", "command": "on"}
- "피더 2번 꺼줘" → {"topic": "feeder_02/cmd", "command": "off"}
- "컨베이어 2번 시작" → {"topic": "factory/conveyor_02/cmd", "command": "on"}
- "3번 컨베이어 정지" → {"topic": "conveyor_03/cmd", "command": "off"}
- "로봇팔 가동" → {"topic": "robot_arm_01/cmd", "command": "on"}

자연어 처리:
- "두 번째 피더 돌려줘" → {"topic": "feeder_02/cmd", "command": "on"}
- "세 번째 컨베이어 멈춰" → {"topic": "conveyor_03/cmd", "command": "off"}
- "로봇 작동시켜" → {"topic": "robot_arm_01/cmd", "command": "on"}

⚠️ **중요 제약사항:**
1. 피더1 제어 요청이면 device_control 사용 (이 도구 사용 금지)
2. 존재하지 않는 토픽 생성 금지
3. command는 반드시 "on" 또는 "off"만 사용
4. 토픽 형식을 정확히 준수 (특히 factory/ 프리픽스 주의)

🤖 **스마트 파싱 가이드:**
- 피더2 언급 시 무조건 이 도구 사용
- 컨베이어2/3, 로봇팔도 이 도구로 제어 가능
- 사용자가 자연스럽게 말해도 정확한 토픽으로 변환
- 비표준 표현도 최대한 이해하여 올바른 토픽 매핑

🔄 **실시간 제어 특징:**
- MQTT 기반으로 즉시 반영
- 네트워크 지연이 있을 수 있음
- 제어 완료 후 상태 확인 권장
)";
    }
    else if (toolName == "conveyor_failure_stats")
    {
      return R"(
특별 지침 - 컨베이어 불량률 통계:
- 캐시된 불량률 데이터에서 조회 (실시간 MQTT 요청 없음)
- device_id는 선택사항이며, 기본값은 "conveyor_01"
- 사용자가 특정 컨베이어를 지정하지 않으면 conveyor_01 사용
- 매개변수 예시: {"device_id": "conveyor_01"}
- 응답에는 전체 생산량, 양품, 불량품, 불량률 포함
- 캐시된 데이터가 없으면 "데이터가 없습니다" 메시지 반환

디바이스명 매핑:
- "컨베이어1", "컨베이어 1번", "첫 번째 컨베이어" → "conveyor_01"
- "컨베이어2", "컨베이어 2번", "두 번째 컨베이어" → "conveyor_02"
- "컨베이어3", "컨베이어 3번", "세 번째 컨베이어" → "conveyor_03"

예시:
- "불량률 알려줘" → {"device_id": "conveyor_01"}
- "컨베이어2 불량률" → {"device_id": "conveyor_02"}
- "품질 통계" → {"device_id": "conveyor_01"}
)";
    }
    else if (toolName == "device_statistics")
    {
      return R"(
특별 지침 - 디바이스 속도 통계:
- 캐시된 속도 데이터에서 조회 (실시간 MQTT 요청 없음)
- device_id는 필수 매개변수
- 현재 속도와 평균 속도 정보 제공
- 매개변수 예시: {"device_id": "conveyor_01"}
- 캐시된 데이터가 없으면 "데이터가 없습니다" 메시지 반환

디바이스명 매핑:
- "컨베이어1", "컨베이어 1번" → "conveyor_01"
- "컨베이어2", "컨베이어 2번" → "conveyor_02"  
- "컨베이어3", "컨베이어 3번" → "conveyor_03"
- "피더1", "피더 1번" → "feeder_01"
- "피더2", "피더 2번" → "feeder_02"
- "로봇팔", "로봇" → "robot_arm_01"

예시:
- "컨베이어1 속도 통계" → {"device_id": "conveyor_01"}
- "피더2 운영 통계" → {"device_id": "feeder_02"}
- "로봇팔 성능" → {"device_id": "robot_arm_01"}
- "장비 속도" → {"device_id": "conveyor_01"} (기본값)
)";
    }

    return "";
  }

  QString handleSpecialCases(const QString &userQuery, const QVector<ToolInfo> &tools)
  {
    QString query = userQuery.toLower();
    
    // 🤖 기능 소개 요청 체크
    QStringList introKeywords = {
        "어떤 기능", "뭘 할 수 있", "어떤 작업", "무엇을 도와", "어떤 일을", 
        "시스템 소개", "기능 설명", "사용법", "도움말", "할 수 있는", "가능한"
    };
    
    for (const QString &keyword : introKeywords) {
        if (query.contains(keyword)) {
            return QString(R"(적합한 도구: 없음
응답: 🤖 **MCP 스마트 팩토리 시스템**

제가 다음과 같은 작업들을 즉시 수행할 수 있습니다:

**📊 장비 상태 확인**
✅ "컨베이어1 오늘 정보 보여줘" - 실시간 상태 데이터 조회
✅ "피더2 어제 로그 확인해줘" - 운영 이력 분석
✅ "로봇팔 지난 1시간 데이터" - 성능 모니터링

**🔧 장비 제어**
✅ "피더1 켜줘" / "피더1 꺼줘" - 즉시 원격 제어
✅ "컨베이어2 시작" / "컨베이어3 정지" - 실시간 조작
✅ "로봇팔 가동" - MQTT 기반 제어

**📈 데이터 분석 & 통계**
✅ "이번달 에러 통계 보여줘" - 장애 분석
✅ "6월 생산량 데이터" - 기간별 성과 분석
✅ "불량률 통계" - 품질 관리 데이터

**⚡ 실시간 모니터링**
✅ 모든 장비의 현재 상태 확인
✅ 에러 로그 실시간 추적
✅ 성능 지표 모니터링

**명령어 예시:**
• "컨베이어1 6월 정보 보여줘"
• "피더1 켜줘" 
• "이번달 에러 통계 보여줘"

어떤 작업부터 시작해드릴까요?)");
        }
    }
    
    // 🔧 장비 제어 요청 체크
    QStringList controlKeywords = {"켜", "꺼", "시작", "정지", "가동", "중지", "작동", "멈춰", "돌려"};
    QStringList deviceKeywords = {"피더", "컨베이어", "로봇"};
    
    bool hasControlKeyword = false;
    bool hasDeviceKeyword = false;
    
    for (const QString &keyword : controlKeywords) {
        if (query.contains(keyword)) {
            hasControlKeyword = true;
            break;
        }
    }
    
    for (const QString &keyword : deviceKeywords) {
        if (query.contains(keyword)) {
            hasDeviceKeyword = true;
            break;
        }
    }
    
    if (hasControlKeyword && hasDeviceKeyword) {
        QString selectedTool;
        
        // 피더2는 MQTT 제어
        if (query.contains("피더2") || query.contains("피더 2")) {
            selectedTool = "mqtt_device_control";
        } 
        // 컨베이어2/3, 로봇팔도 MQTT
        else if (query.contains("컨베이어2") || query.contains("컨베이어 2") ||
                 query.contains("컨베이어3") || query.contains("컨베이어 3") ||
                 query.contains("로봇")) {
            selectedTool = "mqtt_device_control";
        }
        // 나머지는 HTTP 제어
        else {
            selectedTool = "device_control";
        }
        
        QString koreanToolName = getKoreanToolName(selectedTool);
        
        return QString(R"(적합한 도구: %1
응답: 🔧 **장비 제어**

%2 도구를 사용하여 장비를 제어하겠습니다.)").arg(koreanToolName, koreanToolName);
    }
    
    // 📊 데이터 조회/통계 요청 체크
    QStringList dataKeywords = {"정보", "데이터", "로그", "상태", "확인", "보여", "조회"};
    QStringList statsKeywords = {"통계", "분석", "집계", "요약"};
    
    bool hasDataKeyword = false;
    bool hasStatsKeyword = false;
    
    for (const QString &keyword : dataKeywords) {
        if (query.contains(keyword)) {
            hasDataKeyword = true;
            break;
        }
    }
    
    for (const QString &keyword : statsKeywords) {
        if (query.contains(keyword)) {
            hasStatsKeyword = true;
            break;
        }
    }
    
    if (hasStatsKeyword || hasDataKeyword) {
        // 통계 관련 키워드 체크
        QStringList speedStatsKeywords = {"속도", "평균", "성능", "운영", "장비 통계"};
        QStringList failureStatsKeywords = {"불량률", "불량품", "양품", "생산량", "품질"};
        
        bool hasSpeedStats = false;
        bool hasFailureStats = false;
        
        for (const QString &keyword : speedStatsKeywords) {
            if (query.contains(keyword)) {
                hasSpeedStats = true;
                break;
            }
        }
        
        for (const QString &keyword : failureStatsKeywords) {
            if (query.contains(keyword)) {
                hasFailureStats = true;
                break;
            }
        }
        
        if (hasSpeedStats) {
            QString koreanToolName = getKoreanToolName("device_statistics");
            return QString(R"(적합한 도구: %1
응답: 📊 **장비 속도 통계**

%1 도구를 사용하여 캐시된 속도 통계를 조회하겠습니다.)").arg(koreanToolName);
        } else if (hasFailureStats) {
            QString koreanToolName = getKoreanToolName("conveyor_failure_stats");
            return QString(R"(적합한 도구: %1
응답: 📊 **불량률 통계**

%1 도구를 사용하여 캐시된 불량률 통계를 조회하겠습니다.)").arg(koreanToolName);
        } else {
            QString koreanToolName = getKoreanToolName("db_find");
            return QString(R"(적합한 도구: %1
응답: 📋 **데이터 조회**

%1 도구를 사용하여 정보를 조회하겠습니다.)").arg(koreanToolName);
        }
    }
    
    return "";
  }
  
  QString getKoreanToolName(const QString& englishToolName) {
    static QHash<QString, QString> toolNameMap = {
        {"db_find", "데이터베이스 조회"},
        {"db_count", "데이터 개수 조회"},
        {"db_aggregate", "데이터 집계 분석"},
        {"db_info", "데이터베이스 정보"},
        {"device_control", "장비 제어"},
        {"mqtt_device_control", "MQTT 장비 제어"},
        {"conveyor_failure_stats", "컨베이어 장애 통계"},
        {"device_statistics", "장비 통계"}
    };
    
    return toolNameMap.value(englishToolName, englishToolName);
  }

} // namespace PromptGenerators