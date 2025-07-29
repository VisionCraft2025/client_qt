# 스마트 팩토리 MCP Agent 챗봇 시스템 문서

## 목차
1. [시스템 개요](#시스템-개요)
2. [주요 컴포넌트](#주요-컴포넌트)
3. [Gemini API 파이프라인](#gemini-api-파이프라인)
4. [로딩 애니메이션 문제 및 해결방안](#로딩-애니메이션-문제-및-해결방안)
5. [데이터 흐름](#데이터-흐름)
6. [MQTT 통합](#mqtt-통합)

---

## 시스템 개요

스마트 팩토리 MCP Agent 챗봇은 Google Gemini API를 활용하여 자연어 명령을 이해하고, MCP(Machine Control Protocol) 서버와 통신하여 공장 장비를 제어하고 모니터링하는 시스템입니다.

### 주요 기능
- 🤖 자연어 처리를 통한 장비 제어
- 📊 실시간 데이터 조회 및 통계 분석
- 🔧 MQTT/HTTP 기반 장비 제어
- 📈 불량률 및 성능 통계 모니터링

### 기술 스택
- **Frontend**: Qt/C++ 기반 GUI
- **AI**: Google Gemini API (gemini-2.5-flash)
- **Backend**: MCP Server (HTTP REST API)
- **통신**: MQTT, HTTP
- **데이터베이스**: MongoDB (MCP 서버 측)

---

## 주요 컴포넌트

### 1. ChatBotWidget
사용자 인터페이스를 담당하는 위젯으로, 채팅 형식의 대화창을 제공합니다.

**주요 기능:**
- 메시지 표시 (사용자/봇 구분)
- 빠른 응답 버튼
- MQTT 클라이언트 통합
- 드래그 가능한 플로팅 윈도우

### 2. MCPAgentClient
Gemini API와 MCP 서버 간의 통신을 관리하는 핵심 클라이언트입니다.

**주요 기능:**
- 통합 파이프라인 실행
- 도구 목록 관리 및 캐싱
- Gemini API 요청/응답 처리
- MQTT 데이터 캐싱

### 3. DataFormatter
MCP 도구 실행 결과를 사용자 친화적인 형식으로 변환합니다.

**포맷팅 기능:**
- 로그 조회 결과 (날짜별 그룹핑)
- 에러 통계 분석
- 장비별 통계
- MQTT 제어 결과

### 4. PromptGenerators
Gemini API에 전송할 프롬프트를 생성하는 유틸리티입니다.

---

## Gemini API 파이프라인

### 파이프라인 상태
```cpp
enum class PipelineState {
    IDLE,                    // 대기 중
    DISCOVERING_TOOL,        // 도구 분석 중
    PREPARING_EXECUTION,     // 실행 준비 중
    EXECUTING_TOOL,          // 도구 실행 중
    FORMATTING_RESULT        // 결과 포맷팅 중
};
```

### 통합 파이프라인 처리 과정

#### 1. **파이프라인 시작** (`executeUnifiedPipeline`)
```cpp
void MCPAgentClient::executeUnifiedPipeline(const QString& userQuery) {
    // 1. 컨텍스트 생성/업데이트
    if (!m_currentContext) {
        m_currentContext = std::make_unique<ConversationContext>();
    }
    m_currentContext->userQuery = userQuery;
    
    // 2. 도구 목록 확인
    if (!m_toolsCache.has_value()) {
        fetchTools();
    } else {
        // 3. 통합 프롬프트 생성 및 Gemini 요청
        QString prompt = generateUnifiedPrompt(userQuery);
        setPipelineState(PipelineState::DISCOVERING_TOOL);
        sendGeminiRequest(prompt);
    }
}
```

#### 2. **통합 프롬프트 생성** (`generateUnifiedPrompt`)
사용자 쿼리와 사용 가능한 도구 목록을 기반으로 Gemini가 이해할 수 있는 프롬프트를 생성합니다.

**프롬프트 구조:**
```
당신은 스마트 팩토리 AI 어시스턴트입니다.

사용자 요청: "{사용자 입력}"

사용 가능한 도구:
- db_find: 데이터베이스 조회
- device_control: 장비 제어
- mqtt_device_control: MQTT 장비 제어
...

응답 형식 (JSON):
{
    "analysis": "요청 분석 내용",
    "requiresTool": true/false,
    "selectedTool": "도구명 또는 null",
    "toolParameters": { 매개변수 객체 },
    "userMessage": "사용자에게 보여줄 메시지"
}
```

#### 3. **Gemini API 요청** (`sendGeminiRequest`)
```cpp
void MCPAgentClient::sendGeminiRequest(const QString& prompt) {
    // 로딩 애니메이션 시작
    m_loadingTimer->start();
    
    // API 요청 생성
    QString url = QString("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%1")
        .arg(m_geminiApiKey);
    
    // JSON 요청 본문 구성
    QJsonObject requestBody;
    // ... (contents 구성)
    
    QNetworkReply* reply = m_networkManager->post(request, doc.toJson());
    connect(reply, &QNetworkReply::finished, this, &MCPAgentClient::handleGeminiReply);
}
```

#### 4. **Gemini 응답 처리** (`handleGeminiReply`)
```cpp
void MCPAgentClient::handleGeminiReply() {
    m_loadingTimer->stop();
    
    // 1. 응답 파싱
    UnifiedResponse response = parseUnifiedResponse(responseText);
    
    // 2. 사용자 메시지 즉시 출력
    emit logMessage(response.userMessage, 0);
    
    // 3. 도구 실행 필요 여부 확인
    if (response.requiresToolExecution && response.selectedTool.has_value()) {
        setPipelineState(PipelineState::EXECUTING_TOOL);
        executeToolWithParameters(response.selectedTool.value(), response.toolParameters.value_or(QJsonObject()));
    } else {
        // 도구 실행 불필요 - 바로 완료
        emit pipelineCompleted(response.userMessage);
        setPipelineState(PipelineState::IDLE);
    }
}
```

#### 5. **도구 실행** (`executeToolWithParameters`)
선택된 도구에 따라 적절한 처리를 수행합니다:

- **MQTT 기반 제어**: 직접 MQTT 메시지 발행
- **캐시된 통계**: 메모리에서 즉시 반환
- **MCP 서버 도구**: HTTP POST 요청

#### 6. **결과 포맷팅 및 완료**
```cpp
void MCPAgentClient::handleExecuteToolReply() {
    // 1. 결과 포맷팅
    setPipelineState(PipelineState::FORMATTING_RESULT);
    QString formattedResult = DataFormatter::formatExecutionResult(
        m_currentContext->selectedTool.value(), resultText);
    
    // 2. 파이프라인 완료
    emit pipelineCompleted(formattedResult);
    setPipelineState(PipelineState::IDLE);
}
```

### 파이프라인 특징

1. **단일 API 호출**: 도구 선택과 매개변수 생성을 한 번의 Gemini API 호출로 처리
2. **컨텍스트 유지**: 대화 기록을 유지하여 연속적인 대화 가능
3. **캐싱 전략**: 도구 목록 및 통계 데이터 캐싱으로 성능 최적화
4. **비동기 처리**: Qt의 시그널/슬롯 메커니즘을 활용한 비동기 처리

---

## 로딩 애니메이션 문제 및 해결방안

### 문제 원인

1. **시그널 미연결**: `logMessage` 시그널이 UI 업데이트와 연결되지 않음
2. **UI 차단**: 동기적 처리로 인한 이벤트 루프 차단
3. **메시지 덮어쓰기**: 로딩 메시지가 다른 메시지로 즉시 덮어써짐

### 해결 방안

#### 1. logMessage 시그널 연결
```cpp
// chatbot_widget.cpp의 setGemini 함수에 추가
connect(mcpClient.get(), &MCPAgentClient::logMessage,
        this, [this](const QString &message, int level) {
    // 임시 상태 메시지 표시 (별도 UI 요소 사용)
    if (message.contains("생각 중")) {
        // 상태 라벨에 표시하거나 임시 메시지로 처리
        m_statusLabel->setText(message);
    } else {
        // 일반 메시지는 채팅에 추가
        ChatMessage botMsg = {"bot", message, getCurrentTime()};
        addMessage(botMsg);
    }
});
```

#### 2. 별도의 로딩 인디케이터 추가
```cpp
// ChatBotWidget에 로딩 표시 위젯 추가
class ChatBotWidget : public QWidget {
private:
    QLabel* m_loadingIndicator;
    
public:
    void showLoading(bool show) {
        m_loadingIndicator->setVisible(show);
        if (show) {
            // 로딩 애니메이션 시작
            m_loadingAnimation->start();
        } else {
            m_loadingAnimation->stop();
        }
    }
};
```

#### 3. 타이핑 효과 구현
```cpp
void ChatBotWidget::showTypingIndicator() {
    ChatMessage typingMsg = {"bot", "💭 입력 중...", getCurrentTime()};
    m_typingMessageId = addMessage(typingMsg);
}

void ChatBotWidget::hideTypingIndicator() {
    if (m_typingMessageId >= 0) {
        removeMessage(m_typingMessageId);
        m_typingMessageId = -1;
    }
}
```

---

## 데이터 흐름

### 1. 사용자 입력 → Gemini API
```
사용자 입력 → ChatBotWidget::handleSend() 
→ MCPAgentClient::executeUnifiedPipeline()
→ generateUnifiedPrompt() 
→ sendGeminiRequest()
```

### 2. Gemini 응답 → 도구 실행
```
Gemini 응답 → parseUnifiedResponse()
→ executeToolWithParameters()
→ MCP Server 또는 MQTT 또는 캐시
```

### 3. 결과 → 사용자 표시
```
도구 실행 결과 → DataFormatter::formatExecutionResult()
→ pipelineCompleted 시그널
→ ChatBotWidget::addMessage()
```

---

## MQTT 통합

### MQTT 토픽 구조
- **제어 명령**: `{device_id}/cmd` (예: `feeder_02/cmd`)
- **상태 응답**: `{device_id}/status`
- **통계 데이터**: `factory/+/msg/statistics`
- **불량률 정보**: `factory/+/log/info`

### 실시간 데이터 캐싱
```cpp
// 통계 데이터 수신 시 자동 캐싱
void ChatBotWidget::onMqttMessageReceived(const QMqttMessage& message) {
    if (topic.contains("/msg/statistics")) {
        // MCPAgentClient에 데이터 캐싱
        mcpClient->cacheStatisticsData(deviceId, avgSpeed, currentSpeed);
    }
}
```

### 장비 제어 흐름
1. 사용자 명령 → Gemini가 MQTT 도구 선택
2. MQTT 명령 발행 (`on`/`off`)
3. 장비 상태 응답 대기 (5초 타임아웃)
4. 성공/실패 메시지 표시

---

## 주요 도구 목록

### 1. 데이터베이스 도구
- `db_find`: 로그 조회
- `db_count`: 데이터 개수 확인
- `db_aggregate`: 통계 집계
- `db_info`: DB 정보 조회

### 2. 장비 제어 도구
- `device_control`: HTTP 기반 제어
- `mqtt_device_control`: MQTT 기반 제어

### 3. 통계 도구
- `device_statistics`: 장비 속도 통계 (캐시)
- `conveyor_failure_stats`: 불량률 통계 (캐시)

---

## 개선 제안

1. **로딩 상태 표시**: 별도의 로딩 인디케이터 UI 추가
2. **프로그레스 바**: 파이프라인 진행 상황 시각화
3. **에러 복구**: 네트워크 오류 시 재시도 메커니즘
4. **대화 컨텍스트 저장**: 세션 간 대화 기록 유지
5. **도구 추천**: 사용자 쿼리에 따른 도구 추천 기능