# include <Siv3D.hpp> // Siv3D v0.6.16

struct Player
{
	Vec2 pos; // 位置
	Vec2 velocity; // 速度

	double radius = 35.0;

	void update(double wheel, double mouseX, double lagTimer, const RectF& stage)
	{
		// y移動（スクロール）
		const double accelY = wheel * 1.3; // 加速度
		velocity.y += accelY;

		velocity.y *= 0.911; // 摩擦係数
		pos.y += velocity.y;

		// x移動（カーソル）
		pos.x = (lagTimer <= 0.0) ? Math::Lerp(pos.x, mouseX, 0.01) : pos.x;


		// 浮力
		const double centerY = stage.y + 100;
		pos.y = Math::Lerp(pos.y, centerY, 0.007);

		pos.x = Clamp(pos.x, stage.x + radius, stage.x + stage.w - radius);
		pos.y = Clamp(pos.y, stage.y + radius, stage.y + stage.h - radius);
	}

	void draw() const
	{
		// プレイヤー描画（仮）
		Circle{ pos, radius }.draw(Palette::Orange);
	}
};


struct Obstacle
{
	Vec2 pos;
	double baseRadius;
	double radius;
	bool isAlive = true;
	double explosionTime = 0.0;
	double moveTime = 0.0;
	double xBaseOffset = 0.0;

	double blinkTimer = 0.0;
	double blinkProgress = 0.0; // 瞬きアニメーション進行度（0.0～1.0）
	bool isBlinking = false;


	Obstacle(Vec2 startPos, double r)
	{
		pos = startPos;
		baseRadius = r;
		radius = r;
		xBaseOffset = Random(0.0, Math::TwoPi);
		blinkTimer = Random(0.5, 4.0);
	}

	void update(double scrollVel, const Vec2& playerPos)
	{
		pos.y -= scrollVel;
		pos.y -= Scene::DeltaTime() * 20.0;

		moveTime += Scene::DeltaTime();
		pos.x += Sin(moveTime * 1.5 + xBaseOffset) * 0.2;

		// プレイヤーとの距離計算
		double dy = Abs(playerPos.y - pos.y);
		
		double t = Clamp(1.0 - (dy / 1000.0), 0.0, 1.0);

		radius = Math::Lerp(baseRadius, baseRadius * 2.5, t);


		// 瞬き
		if (!isBlinking)
		{
			blinkTimer -= Scene::DeltaTime();
			if (blinkTimer <= 0.0)
			{
				isBlinking = true;
				blinkProgress = 0.0;
			}
		}
		else
		{
			blinkProgress += Scene::DeltaTime() / 0.3;

			if (1.0 <= blinkProgress)
			{
				isBlinking = false;
				blinkTimer = Random(0.5, 4.0); // 次の瞬きまでの時間を再設定
			}
		}


		if ((Scene::Height() + radius) < pos.y)
		{
			isAlive = false;
		}

		if (0.0 < explosionTime)
		{
			explosionTime += Scene::DeltaTime();

			if (0.4 < explosionTime)
			{
				isAlive = false;
			}
		}
	}

	void explode()
	{
		explosionTime = 0.001; // 爆発開始
	}

	void draw(const Vec2& playerPos) const
	{
		if (0.0 < explosionTime) // 爆発アニメーション
		{
			double size = radius + explosionTime * 100;
			Circle{ pos, size }.drawFrame(6, HSV{ Random(0.0, 360.0), 0.7, 1.0 });
		}
		else
		{
			Circle body{ pos, radius };
			body.draw(ColorF{ 0.5 });

			// 目
			Vec2 eyeCenter = pos + Vec2{ 0, -radius * 0.3 };
			double eyeOuterRadius = radius * 0.35; // 白目
			double eyeInnerRadius = eyeOuterRadius * 0.5; // 黒目

			// プレイヤーの方向ベクトルを取得
			Vec2 toPlayer = (playerPos - eyeCenter).normalized();

			double eyeMoveRange = eyeOuterRadius * 0.4;
			Vec2 pupilPos = eyeCenter + toPlayer * eyeMoveRange;


			// 瞬きアニメーション
			double blinkRatio = 1.0;
			if (isBlinking)
			{
				if (blinkProgress < 0.5)
				{
					blinkRatio = 1.0 - (blinkProgress * 2.0);
				}
				else
				{
					blinkRatio = (blinkProgress - 0.5) * 2.0;
				}
				blinkRatio = Clamp(blinkRatio, 0.0, 1.0);
			}

			const double scaleY = Math::Max(0.05, blinkRatio);
			// 白目
			Ellipse(eyeCenter, eyeOuterRadius, eyeOuterRadius * scaleY).draw(ColorF{ 1.0 });
			// 黒目
			if (0.2 < scaleY)
			{
				Ellipse(pupilPos, eyeInnerRadius, eyeInnerRadius * scaleY).draw(ColorF{ 0.1 });
			}
		}
	}
};


struct Bubble
{
	Vec2 pos;
	double radius;
	double speed;
	double life; // 泡の生存時間

	Bubble(Vec2 start)
	{
		pos = start + Vec2{ Random(-8.0, 8.0), Random(-8.0, 8.0) };
		radius = Random(2.0, 6.0);
		speed = Random(60.0, 100.0);
		life = 1.5;
	}

	bool update(double scrollVel)
	{
		pos.y -= (speed + scrollVel * 15) * Scene::DeltaTime();
		life -= Scene::DeltaTime();
		return (0.0 < life);
	}

	void draw() const
	{
		ColorF color{ 0.8, 0.9, 1.0, life * 0.1 };
		Circle{ pos, radius }.draw(color);
	}
};


void Main()
{
	Window::SetFullscreen(true);
	Scene::SetBackground(ColorF{ 0.1, 0.15, 0.2 });


	// ステージ画面
	const double stageWidth = Scene::Width() / 3;
	const double stageHeight = Scene::Height();
	const double stageX = (Scene::Width() - stageWidth) / 2.0;
	const double stageY = 0.0;
	const RectF stage{ stageX, stageY, stageWidth, stageHeight };


	Player player{ Vec2{ stage.center().x, 100 }};


	double scrollY = 0.0;
	double scrollVelocity = 0.0;
	double scrollAccel = 0.0;

	double lagTimer = 0.0; // 被弾時のラグ


	// 障害物
	double lastSpawnY = 0.0;
	double nextSpawnInterval = Random(300.0, 800.0);


	Array<Obstacle> obstacles;
	Array<Bubble> bubbles;


	// タイムアタックモード
	double totalDepth = 10000.0; // ゴールまでの距離
	double currentDepth = 0.0;
	Stopwatch timer{ StartImmediately::Yes }; // タイム計測


	const Font uiFont{ FontMethod::MSDF, 28, Typeface::Medium };
	const Font goalFont{ FontMethod::MSDF, 80, Typeface::Bold };


	// 背景テクスチャ作成（仮）
	const int texHeight = stageHeight;
	Image gradient(1, texHeight);

	for (int y = 0; y < texHeight; ++y)
	{
		double hue = (y * 360.0 / texHeight);
		gradient[y][0] = HSV{ hue, 0.6, 0.5 };
	}

	const Texture gradTex{ gradient, TextureDesc::Mipped };

	const double tileHeight = texHeight;

	bool gameClear = false;


	while (System::Update())
	{
		if (gameClear)
		{
			goalFont(U"GOAL!").drawAt(Scene::Center(), ColorF{ 1.0, 0.9, 0.3 });
			uiFont(U"Time: {:.2f} sec"_fmt(timer.sF())).drawAt(Scene::Center().x, (Scene::Center().y + 100));

			if (MouseL.down())
			{
				// リスタート
				gameClear = false;
				timer.restart();
				currentDepth = 0.0;
				scrollY = scrollVelocity = 0.0;
				obstacles.clear();
			}
			continue;
		}


		if (0.0 < lagTimer)
		{
			lagTimer -= Scene::DeltaTime();
		}


		// ホイール入力を取得
		const double wheel = (lagTimer <= 0.0) ? Abs(Mouse::Wheel()) : 0.0;

		// プレイヤー更新
		player.update(wheel, Cursor::Pos().x, lagTimer, stage);


		// 背景スクロールの加速・減速
		scrollAccel = wheel * 1.7; // 加速度
		scrollVelocity += scrollAccel;
		scrollVelocity *= 0.97; // 摩擦係数
		scrollY += scrollVelocity;

		// 深度計算
		currentDepth += scrollVelocity * 0.1;
		currentDepth = Clamp(currentDepth, 0.0, totalDepth);

		// ゴール判定
		if (totalDepth <= currentDepth)
		{
			gameClear = true;
			timer.pause();
		}


		// 背景描画
		double offsetY = Math::Fmod(scrollY, tileHeight);
		if (offsetY < 0) offsetY += tileHeight;

		// 画面を埋めるために2枚描画
		gradTex.resized(stageWidth, tileHeight).draw(stageX, -offsetY);
		gradTex.resized(stageWidth, tileHeight).draw(stageX, -offsetY + tileHeight);



		// 泡の生成
		double speedAbs = Abs(scrollVelocity); // スクロールスピードの絶対値
		if (1.0 < speedAbs)
		{
			int bubbleCount = static_cast<int>(Min(speedAbs * 0.1, 5.0));
			for (int i = 0; i < bubbleCount; ++i)
			{
				bubbles << Bubble{ player.pos };
			}
		}

		for (auto& b : bubbles)
		{
			b.update(scrollVelocity);
			b.draw();
		}

		bubbles.remove_if([](const Bubble& b) { return b.life <= 0.0; });



		// 障害物の生成
		if (nextSpawnInterval <= scrollY - lastSpawnY)
		{
			static double lastSpawnX = Scene::Center().x;

			double x = 0.0;
			const double minDistanceX = 150.0; // 直前の敵との最低距離

			for (int i = 0; i < 15; ++i)
			{
				double candidateX = Random((stageX + 50.0), (stageX + stageWidth - 50.0));

				if (minDistanceX <= Abs(candidateX - lastSpawnX))
				{
					x = candidateX;
					break;
				}

				// 条件を満たす座標（ランダム）が見つからなかったら、最後の試行で採用
				if (i == 14)
				{
					x = candidateX;
				}
			}

			obstacles << Obstacle{ Vec2{ x, Scene::Height() }, 40 };

			lastSpawnX = x;
			lastSpawnY = scrollY;

			nextSpawnInterval = Random(150.0, 500.0); // 障害物の出現インターバル

			// デバッグ（インターバル確認）
			// Print << U"Spawn at scrollY: " << scrollY;
		}


		for (auto& ob : obstacles)
		{
			ob.update(scrollVelocity, player.pos);
			ob.draw(player.pos);

			if (ob.isAlive && ob.explosionTime == 0.0 && Circle{ player.pos, player.radius }.intersects(Circle{ ob.pos, ob.radius }))
			{
				ob.explode();

				lagTimer = 1.5;

				scrollVelocity *= 0.3; // 慣性演出
			}
		}

		obstacles.remove_if([](const Obstacle& ob) { return !ob.isAlive; });



		// プレイヤー描画
		player.draw();

		// マップ
		const double mapX = (stageX + stageWidth + 40);
		const double mapTop = 100;
		const double mapBottom = Scene::Height() - 100;
		const double mapHeight = mapBottom - mapTop;

		RectF{ mapX, mapTop, 20, mapHeight }.draw(ColorF{ 0.1, 0.4, 0.6, 0.3 });
		double markerY = mapTop + (currentDepth / totalDepth) * mapHeight;
		RectF{ mapX, markerY - 5, 20, 10 }.draw(ColorF{ 1.0, 0.9, 0.3 });


		// UI表示
		uiFont(Format(static_cast<int>(currentDepth), U" m"))
			.draw(30, Arg::topRight(mapX + 70, markerY - 45), ColorF{0.9});

		uiFont(U"Time: {:.2f} sec"_fmt(timer.sF())).draw(60, 20, 60, ColorF{ 0.9 });
	}
}
