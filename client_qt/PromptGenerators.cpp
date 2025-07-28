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
- 로그, 데이터, 통계, 분석, 정보 관련 질문은 db_ 도구를 사용합니다
- 현재 날짜: %1년 %2월 %3일
- 시간/날짜 관련 로그 조회는 db_count나 db_find를 사용합니다
- 통계나 분포 분석은 db_aggregate를 사용합니다
- 데이터베이스 구조나 정보는 db_info를 사용합니다
- 에러, 오류 관련 통계도 db_find나 db_aggregate를 사용합니다

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
- 속도, 평균 속도, 운영 통계 → device_statistics 사용  
- 시간 범위 지정 가능: "오늘", "지난 1시간", 특정 날짜 등
- 실시간 데이터 조회 가능
)";
    }

    // 특별 케이스 처리
    QString specialCaseResponse = handleSpecialCases(userQuery, tools);
    if (!specialCaseResponse.isEmpty())
    {
      return specialCaseResponse;
    }

    QString prompt = QString(R"(사용자의 요청을 분석하고 적절한 도구가 있는지 확인해주세요.

사용자 요청: "%1"

사용 가능한 도구:
%2
%3

분석 규칙:
1. 사용자가 데이터를 "보여달라", "확인하고 싶다", "조회하고 싶다" 등의 표현을 사용하면 도구를 즉시 실행해야 합니다.
2. 로그 개수 규칙:
   - "최근", "요일" 관련 요청: 기본 10개
   - "월" 관련 요청: 기본 20개  
   - "모든", "전체" 요청: 최대 100개
   - "N개" 명시된 경우: N개 (최대 100개)
3. 항상 최신 로그부터 정렬합니다.

특별 케이스 매핑:
- "컨베이어1" → "conveyor_01"
- "피더1" → "feeder_01" 
- "6월" → 2025-06-01부터 2025-06-30까지
- "오늘" → 현재 날짜
- "이번달" → 2025-07-01부터 현재까지

다음 형식으로 응답해주세요:
1. 요청 분석: [사용자가 원하는 작업 설명]
2. 적합한 도구: [도구 이름 또는 "없음"]
3. 이유: [선택한 이유]
4. 응답: [사용자에게 보여줄 응답]

중요: 데이터 조회/확인 요청이면 "도구를 실행하겠습니다"라고 응답하고, 도구 이름을 명시하세요.)")
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
    else if (toolName == "db_aggregate")
    {
        return QString(R"(
특별 지침 - 에러 통계 집계:

"이번달 에러 통계" 요청 시 다음 파이프라인 사용:
{
    "collection": "logs_all",
    "pipeline": [
        {
            "$match": {
                "timestamp": {
                    "$gte": new Date("%1-%2-01T00:00:00Z"),
                    "$lt": new Date("%1-%3-01T00:00:00Z")
                },
                "$or": [
                    {"level": "error"},
                    {"level": "ERROR"},
                    {"log_code": {"$in": ["SPD", "TMP", "MTR", "SNR", "COM", "COL", "EMG", "VIB", "PWR", "OVL"]}}
                ]
            }
        },
        {
            "$group": {
                "_id": {
                    "device_id": "$device_id",
                    "error_code": "$log_code"
                },
                "count": {"$sum": 1}
            }
        },
        {
            "$group": {
                "_id": "$_id.device_id",
                "total_errors": {"$sum": "$count"},
                "error_details": {
                    "$push": {
                        "code": "$_id.error_code",
                        "count": "$count"
                    }
                }
            }
        },
        {
            "$sort": {"total_errors": -1}
        }
    ]
}

중요: 
- 날짜는 ISO 형식으로 정확히 지정
- 모든 에러 타입을 포함하도록 $or 조건 사용
- device_id별로 그룹화하여 기기별 통계 제공
- limit 없이 전체 월 데이터 집계

주의: 타임스탬프는 밀리초가 아닌 ISO Date 형식 사용
)").arg(currentYear)
    .arg(currentMonth, 2, 10, QChar('0'))
    .arg(currentMonth + 1, 2, 10, QChar('0'));
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
특별 지침 - 디바이스 제어:
- device_id는 정확한 형식을 사용: conveyor_01, conveyor_02, conveyor_03, feeder_01, feeder_02, robot_arm_01
- command는 "on" 또는 "off"만 가능
- 한글 표현 변환:
  * "컨베이어1", "컨베이어 1번", "1번 컨베이어" → "conveyor_01"
  * "피더1", "피더 1번", "1번 피더" → "feeder_01"
  * "피더2", "피더 2번", "2번 피더" → "feeder_02"
  * "로봇팔", "로봇암", "로봇" → "robot_arm_01"
  * "켜줘", "켜다", "시작", "가동" → "on"
  * "꺼줘", "끄다", "정지", "멈추다" → "off"

정확한 매핑 예시:
- "피더1 켜줘" → {"device_id": "feeder_01", "command": "on"}
- "피더1 꺼줘" → {"device_id": "feeder_01", "command": "off"}
- "컨베이어1 켜줘" → {"device_id": "conveyor_01", "command": "on"}
- "2번 피더 정지" → {"device_id": "feeder_02", "command": "off"}
- "로봇팔 시작" → {"device_id": "robot_arm_01", "command": "on"}

주의: 여러 디바이스를 동시에 제어하려면 각각 별도로 호출해야 합니다.
)";
    }
     else if (toolName == "mqtt_device_control")
    {
        return R"(
특별 지침 - MQTT 디바이스 제어:
- topic: 디바이스에 따라 정확한 MQTT 토픽 사용
  * 피더 2번, 피더2, 피더02: "feeder_02/cmd"
  * 컨베이어 2번, 컨베이어2, 컨베이어02: "factory/conveyor_02/cmd"
  * 컨베이어 3번, 컨베이어3, 컨베이어03: "conveyor_03/cmd"
  * 로봇팔, 로봇암, 로봇: "robot_arm_01/cmd"
- command: "on" 또는 "off"만 가능
- 한글 표현 변환:
  * "켜", "킬", "시작", "가동", "작동" → "on"
  * "꺼", "끄", "정지", "멈춰", "멈추" → "off"

예시:
- "피더2 켜줘" → {"topic": "feeder_02/cmd", "command": "on"}
- "피더 2 꺼줘" → {"topic": "feeder_02/cmd", "command": "off"}
- "컨베이어 2번 시작" → {"topic": "factory/conveyor_02/cmd", "command": "on"}
- "로봇팔 정지" → {"topic": "robot_arm_01/cmd", "command": "off"}

중요: 피더2는 반드시 mqtt_device_control 도구 사용 (device_control이 아님)
)";
    }
    else if (toolName == "conveyor_failure_stats")
    {
      return R"(
특별 지침 - 컨베이어 불량률 통계:
- request_topic: "factory/conveyor_01/log/request"
- response_topic: "factory/conveyor_01/log/info" (응답 대기용)
- 요청 메시지는 빈 객체 {} 전송
- 시간 범위가 필요한 경우 서버에서 기본값 사용
- 응답 데이터 구조:
  * total: 전체 개수
  * pass: 양품 개수
  * fail: 불량품 개수  
  * failure: 불량률 (0.0000 ~ 1.0000)

예시:
- "불량률 알려줘" → {"request_topic": "factory/conveyor_01/log/request"}
)";
    }
    else if (toolName == "device_statistics")
    {
      return R"(
특별 지침 - 디바이스 통계:
- request_topic: "factory/statistics"
- response_topic: "factory/{device_id}/msg/statistics" 
- device_id: "conveyor_01", "feeder_01" 등
- 시간 범위 지정 (밀리초 타임스탬프):
  * "오늘": 오늘 00:00 ~ 현재
  * "지난 1시간": 현재 - 1시간 ~ 현재
  * 특정 날짜: YYYY-MM-DD 형식으로 변환
- time_range: start와 end를 밀리초 타임스탬프로 지정

예시:
- "오늘 컨베이어 통계" → {
    "device_id": "conveyor_01",
    "time_range": {
      "start": [오늘 00:00의 타임스탬프],
      "end": [현재 타임스탬프]
    }
  }

현재 시간 기준으로 타임스탬프 계산:
- 현재: QDateTime::currentMSecsSinceEpoch()
- 오늘 시작: QDateTime(QDate::currentDate(), QTime(0,0,0)).toMSecsSinceEpoch()
- 1시간 전: QDateTime::currentDateTime().addSecs(-3600).toMSecsSinceEpoch()
)";
    }

    return "";
  }

  QString handleSpecialCases(const QString &userQuery, const QVector<ToolInfo> &tools)
  {
    QString query = userQuery.trimmed();

    // 1) "어떤거 할 수 있어?" 케이스
    if (query.contains("어떤") && (query.contains("할 수") || query.contains("가능")))
    {
      QStringList capabilities;

      // 사용 가능한 도구별 기능 설명
      bool hasDbTools = false;
      bool hasDeviceControl = false;
      bool hasMqttControl = false;
      bool hasFailureStats = false;
      bool hasDeviceStats = false;

      for (const auto &tool : tools)
      {
        if (tool.name.startsWith("db_"))
        {
          hasDbTools = true;
        }
        else if (tool.name == "device_control")
        {
          hasDeviceControl = true;
        }
        else if (tool.name == "mqtt_device_control")
        {
          hasMqttControl = true;
        }
        else if (tool.name == "conveyor_failure_stats")
        {
          hasFailureStats = true;
        }
        else if (tool.name == "device_statistics")
        {
          hasDeviceStats = true;
        }
      }

      QString response = QString(R"(🤖 **MCP 스마트 팩토리 시스템**

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

      return response;
    }

    // 2-5) 특정 케이스들에 대한 힌트 제공
    if (query.contains("컨베이어1") && query.contains("6월"))
    {
      return "🔍 **컨베이어1 6월 정보 조회**\n\ndb_find 도구를 사용하여 2025년 6월 전체 기간의 conveyor_01 로그를 조회하겠습니다.";
    }

    if (query.contains("컨베이어1") && query.contains("오늘"))
    {
      return "🔍 **컨베이어1 오늘 정보 조회**\n\ndb_find 도구를 사용하여 오늘(2025-07-28) conveyor_01 로그를 조회하겠습니다.";
    }

    if (query.contains("피더1") && (query.contains("켜") || query.contains("꺼")))
    {
      QString action = query.contains("켜") ? "가동" : "정지";
      return QString("🔧 **피더1 %1**\n\ndevice_control 도구를 사용하여 feeder_01을 %2하겠습니다.").arg(action, action);
    }

    if (query.contains("이번달") && query.contains("에러"))
    {
      return QString(R"(1. 요청 분석: 사용자가 이번달(7월) 전체 에러 통계를 원함
2. 적합한 도구: db_aggregate
3. 이유: 기기별, 에러 타입별 통계 집계가 필요하므로 db_aggregate 사용
4. 응답: 📊 **이번달 에러 통계 분석**

db_aggregate 도구를 사용하여 2025년 7월의 기기별, 에러 타입별 통계를 집계하겠습니다.)");
    }

    // "이번달 에러 통계" 케이스 수정 - db_aggregate 사용하도록 강제
    if ((query.contains("이번달") || query.contains("이번 달")) &&
        query.contains("에러") &&
        (query.contains("통계") || query.contains("분석")))
    {
      return QString(R"(1. 요청 분석: 사용자가 이번달(7월) 전체 에러 통계를 원함
2. 적합한 도구: db_aggregate
3. 이유: 기기별, 에러 타입별 통계 집계가 필요하므로 db_aggregate 사용
4. 응답: 📊 **이번달 에러 통계 분석**

db_aggregate 도구를 실행하겠습니다.)");
    }

    // 피더2 제어 케이스 추가
    if ((query.contains("피더2") || query.contains("피더 2")) &&
        (query.contains("켜") || query.contains("꺼")))
    {
      QString action = query.contains("켜") ? "가동" : "정지";
      QString command = query.contains("켜") ? "on" : "off";
      return QString(R"(1. 요청 분석: 사용자가 피더2를 %1하려고 함
2. 적합한 도구: mqtt_device_control
3. 이유: 피더2는 MQTT를 통해 제어 가능
4. 응답: 🔧 **피더2 %1**

mqtt_device_control 도구를 실행하겠습니다.)")
          .arg(action);
    }

    return "";
  }

} // namespace PromptGenerators