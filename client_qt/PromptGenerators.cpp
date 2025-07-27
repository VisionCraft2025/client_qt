#include "PromptGenerators.h"
#include <algorithm>

namespace PromptGenerators {

QString generateToolDiscoveryPrompt(const QString& userQuery, 
                                   const QVector<ToolInfo>& tools) {
    QStringList toolsInfo;
    for (const auto& tool : tools) {
        QString toolStr = QString("• %1: %2").arg(tool.name, tool.description);
        if (!tool.examples.isEmpty()) {
            QStringList examples = tool.examples.mid(0, 3);
            toolStr += QString("\n  예시: %1").arg(examples.join(", "));
        }
        toolsInfo.append(toolStr);
    }
    
    // DB 도구 확인
    bool hasDbTools = std::any_of(tools.begin(), tools.end(), 
        [](const ToolInfo& tool) { return tool.name.startsWith("db_"); });
    
    // 디바이스 제어 도구 확인
    bool hasDeviceControl = std::any_of(tools.begin(), tools.end(),
        [](const ToolInfo& tool) { return tool.name == "device_control"; });
    
    // MQTT 도구 확인
    bool hasMqttDeviceControl = std::any_of(tools.begin(), tools.end(),
        [](const ToolInfo& tool) { return tool.name == "mqtt_device_control"; });
    
    bool hasFailureStats = std::any_of(tools.begin(), tools.end(),
        [](const ToolInfo& tool) { return tool.name == "conveyor_failure_stats"; });
    
    bool hasDeviceStats = std::any_of(tools.begin(), tools.end(),
        [](const ToolInfo& tool) { return tool.name == "device_statistics"; });
    
    QString contextInfo;
    
    if (hasDbTools) {
        contextInfo += R"(
특별 참고사항 - 데이터베이스:
- 로그, 데이터, 통계, 분석 관련 질문은 db_ 도구를 사용합니다
- 시간/날짜 관련 로그 조회는 db_count나 db_find를 사용합니다
- 통계나 분포 분석은 db_aggregate를 사용합니다
- 데이터베이스 구조나 정보는 db_info를 사용합니다

디바이스 이름 매핑:
- 컨베이어, 컨베이어벨트 → conveyor_01, conveyor_02, conveyor_03
- 피더, 공급장치 → feeder_01, feeder_02
- 로봇팔, 로봇암, 로봇 → robot_arm_01
- "1번", "2번", "3번" 같은 숫자는 해당 device_id의 번호로 변환
)";
    }
    
    if (hasDeviceControl) {
        contextInfo += R"(
특별 참고사항 - 디바이스 제어 (HTTP):
- 켜다, 시작, 가동, 작동 → "on" 명령
- 끄다, 정지, 멈추다, 중지 → "off" 명령
- 디바이스 타입: 컨베이어(3대), 피더(2대), 로봇팔(1대)
- "모든", "전체", "전부" 키워드가 있으면 해당 타입의 모든 디바이스 제어
)";
    }
    
    if (hasMqttDeviceControl) {
        contextInfo += R"(
특별 참고사항 - MQTT 디바이스 제어:
- MQTT를 통한 실시간 디바이스 제어
- 제어 가능 디바이스:
  * 컨베이어 3번: conveyor_03/cmd
  * 컨베이어 2번: factory/conveyor_02/cmd  
  * 피더 2번: feeder_02/cmd
  * 로봇팔 1번: robot_arm_01/cmd
- "MQTT로", "실시간으로" 같은 키워드가 있으면 이 도구 사용
)";
    }
    
    if (hasFailureStats || hasDeviceStats) {
        contextInfo += R"(
특별 참고사항 - 통계 데이터:
- 불량률, 양품, 불량품, 생산량 관련 → conveyor_failure_stats 사용
- 속도, 평균 속도, 운영 통계 → device_statistics 사용  
- 시간 범위 지정 가능: "오늘", "지난 1시간", 특정 날짜 등
- 실시간 데이터 조회 가능
)";
    }
    
    QString prompt = QString(R"(사용자의 요청을 분석하고 적절한 도구가 있는지 확인해주세요.

사용자 요청: "%1"

사용 가능한 도구:
%2
%3
다음 형식으로 응답해주세요:
1. 요청 분석: [사용자가 원하는 작업 설명]
2. 적합한 도구: [도구 이름 또는 "없음"]
3. 이유: [선택한 이유 또는 없는 이유]
4. 응답: [사용자에게 보여줄 최종 응답]

만약 적합한 도구가 있다면, 응답에 구체적인 사용 예시를 포함해주세요.)")
        .arg(userQuery)
        .arg(toolsInfo.join("\n"))
        .arg(contextInfo);
    
    return prompt;
}

QString generateToolExecutionPrompt(ConversationContext* context) {
    auto toolIt = std::find_if(context->availableTools.begin(), context->availableTools.end(),
        [context](const ToolInfo& tool) { 
            return tool.name == context->selectedTool.value(); 
        });
    
    if (toolIt == context->availableTools.end()) {
        return "";
    }
    
    const ToolInfo& tool = *toolIt;
    
    // 대화 맥락 구성
    QString conversationContext;
    int startIdx = std::max(0, static_cast<int>(context->conversationHistory.size()) - 3);
    for (int i = startIdx; i < context->conversationHistory.size(); ++i) {
        const auto& msg = context->conversationHistory[i];
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

QString getSpecialInstructions(const QString& toolName) {
    if (toolName.startsWith("db_")) {
        return R"(
특별 지침 - 데이터베이스:
- collection이 명시되지 않았으면 "logs_all"을 사용하세요
- 날짜는 반드시 "YYYY-MM-DD" 형식으로 지정하세요
- limit이 명시되지 않았으면 5를 사용하세요
- query_type이나 aggregate_type은 정확히 지정된 값 중 하나를 사용하세요

디바이스명 변환 규칙:
- "컨베이어", "컨베이어 1번" → "conveyor_01"
- "2번 컨베이어" → "conveyor_02"
- "피더", "1번 피더" → "feeder_01"
- "로봇팔", "로봇" → "robot_arm_01"
)";
    } else if (toolName == "device_control") {
        return R"(
특별 지침 - 디바이스 제어:
- device_id는 정확한 형식을 사용: conveyor_01, conveyor_02, conveyor_03, feeder_01, feeder_02, robot_arm_01
- command는 "on" 또는 "off"만 가능
- 한글 표현 변환:
  * "컨베이어", "컨베이어 1번", "1번 컨베이어" → "conveyor_01"
  * "피더", "공급장치" + 번호 → "feeder_01" 또는 "feeder_02"
  * "로봇팔", "로봇암", "로봇" → "robot_arm_01"
  * "켜다", "시작", "가동" → "on"
  * "끄다", "정지", "멈추다" → "off"

예시:
- "컨베이어 1번 켜줘" → {"device_id": "conveyor_01", "command": "on"}
- "2번 피더 정지" → {"device_id": "feeder_02", "command": "off"}
- "로봇팔 시작" → {"device_id": "robot_arm_01", "command": "on"}

주의: 여러 디바이스를 동시에 제어하려면 각각 별도로 호출해야 합니다.
)";
    } else if (toolName == "mqtt_device_control") {
        return R"(
특별 지침 - MQTT 디바이스 제어:
- topic: 디바이스에 따라 정확한 MQTT 토픽 사용
  * 컨베이어 3번: "conveyor_03/cmd"
  * 컨베이어 2번: "factory/conveyor_02/cmd"  
  * 피더 2번: "feeder_02/cmd"
  * 로봇팔 1번: "robot_arm_01/cmd"
- command: "on" 또는 "off"만 가능
- 한글 표현 변환:
  * "켜다", "시작", "가동" → "on"
  * "끄다", "정지", "멈추다" → "off"

예시:
- "컨베이어 3번 켜줘" → {"topic": "conveyor_03/cmd", "command": "on"}
- "피더 정지" → {"topic": "feeder_02/cmd", "command": "off"}
- "전체 가동" → 각 디바이스별로 별도 호출 필요

주의: 전체 제어 시 각 디바이스에 대해 개별적으로 호출해야 합니다.
)";
    } else if (toolName == "conveyor_failure_stats") {
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
    } else if (toolName == "device_statistics") {
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

} // namespace PromptGenerators